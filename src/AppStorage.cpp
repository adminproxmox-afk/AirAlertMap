#include "AppStorage.h"
#include "UkraineAlarmFeed.h"
#include "MapData.h"
#include "Preferences.h"
#include <System.JSON.hpp>
#include <memory>
#include <set>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#pragma link "shell32.lib"
using namespace System;
using namespace System::Json;
static bool NumericId(const std::wstring& id) {
    if(id.empty() || id.size()>10)return false;
    for(unsigned i=0;i<id.size();++i)if(id[i]<L'0'||id[i]>L'9')return false;
    return true;
}
bool AppStorage::SaveCatalog(const std::vector<TerritoryStatus>& territories,std::string& digest) const {
    if(territories.empty()||territories.size()>20000)return false;
    std::auto_ptr<TJSONObject> catalog(new TJSONObject());
    catalog->AddPair(L"version",new TJSONNumber(1));catalog->AddPair(L"provider",L"ukrainealarm");
    TJSONArray* items=new TJSONArray();catalog->AddPair(L"territories",items);
    std::set<std::wstring> ids;
    for(unsigned i=0;i<territories.size();++i){
        const TerritoryStatus& t=territories[i];
        if(!NumericId(t.id)||(!t.parentId.empty()&&!NumericId(t.parentId))||!ids.insert(t.id).second||t.name.empty()||t.name.size()>512||t.type.empty()||t.type.size()>32||t.oblast<-1||t.oblast>=27)return false;
        TJSONObject* item=new TJSONObject();items->AddElement(item);
        item->AddPair(L"id",UnicodeString(t.id.c_str()));item->AddPair(L"name",UnicodeString(t.name.c_str()));
        item->AddPair(L"parent",UnicodeString(t.parentId.c_str()));item->AddPair(L"type",UnicodeString(t.type.c_str()));
        item->AddPair(L"oblast",new TJSONNumber(t.oblast));
    }
    UTF8String encoded(catalog->ToJSON());std::string body(encoded.c_str(),encoded.Length());
    unsigned __int64 hash=0xcbf29ce484222325ui64;
    for(unsigned i=0;i<body.size();++i){hash^=(unsigned char)body[i];hash*=0x100000001b3ui64;}
    char hexadecimal[32];sprintf(hexadecimal,"%016I64x",hash);digest=hexadecimal;
    std::wstring name=L"catalog-";for(unsigned i=0;i<digest.size();++i)name+=(wchar_t)digest[i];name+=L".json";
    std::string existing;
    if(GetFileAttributesW((root+L"\\"+name).c_str())!=INVALID_FILE_ATTRIBUTES)
        return Read(name.c_str(),existing,8*1024*1024)&&existing==body;
    return Write(name.c_str(),body);
}
std::string PublicLevels(const AlertSnapshot& snapshot,bool available) {
    std::string levels(RegionCount,'?');
    if(available) for(int i=0;i<RegionCount;++i) {
        if(snapshot.states[i]==asClear) levels[i]='N';
        else if(snapshot.states[i]==asAlert)
            levels[i]=snapshot.levels[i]=='P'?'P':snapshot.levels[i]=='Y'?'Y':'R';
    }
    return levels;
}

__int64 UtcSeconds() {
    FILETIME time; GetSystemTimeAsFileTime(&time);
    ULARGE_INTEGER value; value.LowPart=time.dwLowDateTime; value.HighPart=time.dwHighDateTime;
    return (__int64)(value.QuadPart/10000000)-11644473600i64;
}
static std::string Decimal(__int64 number) {
    char text[40]; sprintf(text,"%I64d",number); return text;
}
bool AppStorage::AppendObservation(const AlertSnapshot& snapshot,bool available,ApiProvider provider,__int64 now) const {
    if(root.empty() || now<0 || now>9999999999i64 || (provider!=apUkraineAlarm && provider!=apAlertsInUa)) return false;
    if(!CreateDirectoryW(root.c_str(),NULL) && GetLastError()!=ERROR_ALREADY_EXISTS) return false;
    std::string detail,catalog;
    if(available && provider==apUkraineAlarm && !snapshot.territories.empty()) {
        if(!SaveCatalog(snapshot.territories,catalog))return false;
        for(unsigned i=0;i<snapshot.territories.size();++i){char c=snapshot.territories[i].level;if(c!='N'&&c!='R'&&c!='Y'&&c!='P'&&c!='?')return false;detail+=c;}
    }
    ULARGE_INTEGER value; value.QuadPart=(now+11644473600i64)*10000000;
    FILETIME fileTime; fileTime.dwLowDateTime=value.LowPart; fileTime.dwHighDateTime=value.HighPart;
    SYSTEMTIME time; if(!FileTimeToSystemTime(&fileTime,&time)) return false;
    wchar_t name[40]; wsprintfW(name,L"\\history-%04u-%02u.jsonl",time.wYear,time.wMonth);
    HANDLE file=CreateFileW((root+name).c_str(),FILE_APPEND_DATA,FILE_SHARE_READ,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE) return false;
    // A leading newline also isolates a partially written last record after a crash.
    std::string line=std::string("\n{\"version\":2,\"at\":")+Decimal(now)+",\"provider\":\""+
        (provider==apUkraineAlarm?"ukrainealarm":"alerts.in.ua")+"\",\"available\":"+(available?"true":"false")+
        ",\"levels\":\""+PublicLevels(snapshot,available)+"\""+
        (catalog.empty()?std::string():",\"catalog\":\""+catalog+"\",\"territoryLevels\":\""+detail+"\"")+"}\n";
    DWORD count=0;
    bool written=WriteFile(file,line.data(),line.size(),&count,NULL)!=FALSE && count==line.size();
    CloseHandle(file); return written;
}
static bool ParseNumber(const std::string& text,__int64& number) {
    if(text.empty() || text.size()>12) return false;
    number=0;
    for(unsigned i=0;i<text.size();++i) {
        if(text[i]<'0' || text[i]>'9') return false;
        number=number*10+text[i]-'0';
    }
    return true;
}
AppStorage::AppStorage(const std::wstring& folder) : root(folder) {
    if(root.empty()) {
        wchar_t path[MAX_PATH];
        if(SUCCEEDED(SHGetFolderPathW(NULL,CSIDL_LOCAL_APPDATA,NULL,SHGFP_TYPE_CURRENT,path)))
            root=std::wstring(path)+L"\\AirAlertMap";
    }
}
bool AppStorage::Read(const wchar_t* name,std::string& body,unsigned maximum) const {
    if(root.empty()) return false;
    FILE* file=_wfopen((root+L"\\"+name).c_str(),L"rb");
    if(!file) return false;
    std::string value; char buffer[4096]; bool valid;
    for(;;) {
        unsigned count=fread(buffer,1,sizeof(buffer),file);
        if(value.size()+count>maximum) { valid=false; break; }
        value.append(buffer,count);
        if(count<sizeof(buffer)) { valid=!ferror(file); break; }
    }
    fclose(file);
    if(valid) body=value;
    return valid;
}
bool AppStorage::Write(const wchar_t* name,const std::string& body) const {
    if(root.empty()) return false;
    if(!CreateDirectoryW(root.c_str(),NULL) && GetLastError()!=ERROR_ALREADY_EXISTS) return false;
    wchar_t temporary[MAX_PATH];
    if(!GetTempFileNameW(root.c_str(),L"aam",0,temporary)) return false;
    HANDLE file=CreateFileW(temporary,GENERIC_WRITE,0,NULL,TRUNCATE_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    bool written=false;
    if(file!=INVALID_HANDLE_VALUE) {
        DWORD count=0;
        written=WriteFile(file,body.data(),body.size(),&count,NULL)!=FALSE && count==body.size() && FlushFileBuffers(file)!=FALSE;
        CloseHandle(file);
    }
    if(written) written=MoveFileExW(temporary,(root+L"\\"+name).c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
    if(!written) DeleteFileW(temporary);
    return written;
}
bool AppStorage::SaveDirectory(const std::string& body,__int64 now) const {
    if(now<0 || PrepareRegionDirectory(body).error!=aePreparing) return false;
    std::string clean=PublicUkraineAlarmDiagnostic(body);
    if(PrepareRegionDirectory(clean).error!=aePreparing) return false;
    return Write(L"regions.cache",std::string("AirAlertMap directory 1\n")+Decimal(now)+"\n"+clean);
}
bool AppStorage::LoadDirectory(std::string& body,__int64 now) const {
    std::string stored;
    const std::string prefix="AirAlertMap directory 1\n";
    if(!Read(L"regions.cache",stored,2*1024*1024+80) || stored.compare(0,prefix.size(),prefix)!=0) return false;
    unsigned end=stored.find('\n',prefix.size());
    if(end==std::string::npos) return false;
    __int64 saved;
    if(!ParseNumber(stored.substr(prefix.size(),end-prefix.size()),saved) || now<saved || now-saved>=86400) return false;
    std::string directory=stored.substr(end+1);
    if(PrepareRegionDirectory(directory).error!=aePreparing) return false;
    body=directory; return true;
}
void AppStorage::InvalidateDirectory() const {
    if(!root.empty()) DeleteFileW((root+L"\\regions.cache").c_str());
}
bool AppStorage::SaveSelectedRegion(int index) const {
    return index>=0 && index<RegionCount && Write(L"settings.txt",std::string("AirAlertMap settings 1\n")+Regions[index].iso);
}
int AppStorage::LoadSelectedRegion(int fallback) const {
    std::string stored;
    if(Read(L"settings.txt",stored,128))
        for(int i=0;i<RegionCount;++i)
            if(stored==std::string("AirAlertMap settings 1\n")+Regions[i].iso) return i;
    return fallback>=0 && fallback<RegionCount?fallback:7;
}
bool AppStorage::SavePreferences(const std::string& json) const {
    std::string normalized;return NormalizePreferences(json,normalized)&&Write(L"preferences.json",normalized);
}
bool AppStorage::LoadPreferences(std::string& json) const {
    std::string body,normalized;if(!Read(L"preferences.json",body,8192)||!NormalizePreferences(body,normalized))return false;
    json=normalized;return true;
}
bool AppStorage::SaveHistoryRetention(unsigned days) const {
    return (days==0||days==90||days==180||days==365)&&Write(L"history-policy.txt",std::string("AirAlertMap retention 1\n")+Decimal(days));
}
unsigned AppStorage::LoadHistoryRetention() const {
    std::string text;const std::string prefix="AirAlertMap retention 1\n";__int64 days;
    if(!Read(L"history-policy.txt",text,64)||text.compare(0,prefix.size(),prefix)!=0||!ParseNumber(text.substr(prefix.size()),days))return 0;
    return days==90||days==180||days==365?(unsigned)days:0;
}
static const wchar_t* RetryFile(ApiProvider provider) {
    return provider==apUkraineAlarm?L"ukrainealarm.retry":L"alerts.retry";
}
bool AppStorage::SaveRetry(ApiProvider provider,unsigned seconds,__int64 now) const {
    if(now<0 || seconds>3600 || (provider!=apUkraineAlarm && provider!=apAlertsInUa)) return false;
    return Write(RetryFile(provider),Decimal(now+seconds));
}
unsigned AppStorage::LoadRetry(ApiProvider provider,__int64 now) const {
    std::string stored; __int64 deadline;
    if(!Read(RetryFile(provider),stored,32) || !ParseNumber(stored,deadline) || deadline<=now || deadline-now>3600) return 0;
    return (unsigned)(deadline-now);
}
