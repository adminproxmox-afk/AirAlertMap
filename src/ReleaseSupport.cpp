#include <vcl.h>
#pragma hdrstop
#include "ReleaseSupport.h"
#include "HistoryReport.h"
#include "../resources/version.h"
#include <System.JSON.hpp>
#include <memory>
#include <stdio.h>
using namespace System::Json;
static bool Digits(const std::wstring& s,unsigned from,unsigned count) {for(unsigned i=from;i<from+count;++i)if(s[i]<L'0'||s[i]>L'9')return false;return true;}
static bool HistoryName(const std::wstring& s) {
    if(s.size()!=21 || s.substr(0,8)!=L"history-" || s[12]!=L'-' || s.substr(15)!=L".jsonl" || !Digits(s,8,4)||!Digits(s,13,2))return false;
    int month=(s[13]-L'0')*10+s[14]-L'0';return month>=1&&month<=12;
}
static bool CatalogName(const std::wstring& s) {
    if(s.size()!=29||s.substr(0,8)!=L"catalog-"||s.substr(24)!=L".json")return false;
    for(unsigned i=8;i<24;++i)if(!((s[i]>=L'0'&&s[i]<=L'9')||(s[i]>=L'a'&&s[i]<=L'f')))return false;return true;
}
HistoryStorageStats InspectHistoryStorage(const std::wstring& folder) {
    HistoryStorageStats stats;if(folder.empty()){stats.error=ERROR_INVALID_PARAMETER;return stats;}
    WIN32_FIND_DATAW file;HANDLE h=FindFirstFileW((folder+L"\\*").c_str(),&file);
    if(h==INVALID_HANDLE_VALUE){DWORD error=GetLastError();if(error!=ERROR_FILE_NOT_FOUND&&error!=ERROR_PATH_NOT_FOUND)stats.error=error;return stats;}
    do {
        if(file.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))continue;
        std::wstring name=file.cFileName;unsigned __int64 size=((unsigned __int64)file.nFileSizeHigh<<32)|file.nFileSizeLow;
        if(HistoryName(name)){stats.bytes+=size;++stats.files;}
        else if(CatalogName(name)){stats.bytes+=size;stats.catalogBytes+=size;++stats.catalogs;}
    }while(FindNextFileW(h,&file));DWORD error=GetLastError();FindClose(h);if(error!=ERROR_NO_MORE_FILES)stats.error=error;return stats;
}
System::UnicodeString BuildDiagnostics(const DiagnosticState& s) {
    std::auto_ptr<TJSONObject> out(new TJSONObject());
    out->AddPair(L"schemaVersion",new TJSONNumber(1));out->AddPair(L"appVersion",System::UnicodeString(APP_VERSION_W));
    out->AddPair(L"provider",System::UnicodeString(s.provider==apUkraineAlarm?L"ukrainealarm":s.provider==apAlertsInUa?L"alerts.in.ua":L"unknown"));
    out->AddPair(L"apiError",new TJSONNumber((int)s.apiError));out->AddPair(L"httpStatus",new TJSONNumber((int)s.httpStatus));out->AddPair(L"networkError",new TJSONNumber((__int64)s.networkError));
    out->AddPair(L"lastConfirmedUtc",new TJSONNumber(s.confirmedAt));out->AddPair(L"retrySeconds",new TJSONNumber((int)s.retrySeconds));
    out->AddPair(L"available",new TJSONBool(s.available));out->AddPair(L"demo",new TJSONBool(s.demo));out->AddPair(L"pythonAvailable",new TJSONBool(s.pythonAvailable));out->AddPair(L"historyWritable",new TJSONBool(s.historyOk));out->AddPair(L"keyConfigured",new TJSONBool(s.keyConfigured));
    out->AddPair(L"historyBytes",new TJSONNumber((__int64)s.storage.bytes));out->AddPair(L"historyFiles",new TJSONNumber((int)s.storage.files));out->AddPair(L"retentionDays",new TJSONNumber((int)s.retentionDays));
    return out->ToJSON();
}
__fastcall TStorageWorker::TStorageWorker(const std::wstring& path,const std::wstring& root,const std::string& operation,unsigned retention,__int64 timestamp,const std::wstring& archive)
 : TThread(true),script(path),folder(root),destination(archive),action(operation),days(retention),now(timestamp),cancel(CreateEventW(NULL,TRUE,FALSE,NULL)),Error(0) {
    FreeOnTerminate=false;wchar_t name[100];wsprintfW(name,L"\\reports\\storage-%lu-%lu.json",GetCurrentProcessId(),GetTickCount());Output=folder+name;
}
__fastcall TStorageWorker::~TStorageWorker(){if(cancel)SetEvent(cancel);if(!Suspended)WaitFor();if(cancel)CloseHandle(cancel);}
void __fastcall TStorageWorker::Execute() {
    if(!cancel){Error=ERROR_NOT_ENOUGH_MEMORY;return;}
    if((action!="archive"&&action!="prune")||(action=="prune"&&days!=90&&days!=180&&days!=365)){Error=ERROR_INVALID_PARAMETER;return;}
    std::vector<std::wstring> args;args.push_back(L"--action");args.push_back(action=="archive"?L"archive":L"prune");args.push_back(L"--history");args.push_back(folder);args.push_back(L"--output");args.push_back(Output);
    wchar_t number[40];swprintf(number,L"%I64d",now);args.push_back(L"--now");args.push_back(number);
    if(action=="archive"){args.push_back(L"--destination");args.push_back(destination);}else{swprintf(number,L"%u",days);args.push_back(L"--days");args.push_back(number);}
    Error=RunPythonTool(script,args,300000,cancel);
}
