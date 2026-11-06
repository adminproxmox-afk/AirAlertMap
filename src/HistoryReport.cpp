#include <vcl.h>
#pragma hdrstop
#include "HistoryReport.h"
#include <wchar.h>
#include <stdio.h>
#include <vector>
#pragma link "advapi32.lib"
#pragma package(smart_init)
std::wstring FindPythonOnPath(const std::wstring& searchPath) {
    unsigned start=0;
    while(start<searchPath.size()) {
        unsigned end=searchPath.find(L';',start);
        if(end==std::wstring::npos) end=searchPath.size();
        std::wstring folder=searchPath.substr(start,end-start); start=end+1;
        if(folder.size()>=2 && folder[0]==L'"' && folder[folder.size()-1]==L'"') folder=folder.substr(1,folder.size()-2);
        bool absolute=(folder.size()>=3 && folder[1]==L':' && (folder[2]==L'\\' || folder[2]==L'/')) ||
                      (folder.size()>2 && folder[0]==L'\\' && folder[1]==L'\\');
        if(!absolute) continue;
        std::wstring executable=folder+(folder[folder.size()-1]==L'\\'?L"python.exe":L"\\python.exe");
        DWORD attributes=GetFileAttributesW(executable.c_str());
        if(attributes!=INVALID_FILE_ATTRIBUTES && !(attributes&FILE_ATTRIBUTE_DIRECTORY)) return executable;
    }
    return L"";
}
static std::wstring RegistryText(HKEY key,const wchar_t* name) {
    wchar_t text[32768]; DWORD type=0,bytes=sizeof(text);
    if(RegQueryValueExW(key,name,NULL,&type,(BYTE*)text,&bytes)!=ERROR_SUCCESS ||
       type!=REG_SZ || bytes<sizeof(wchar_t) || bytes>sizeof(text) || text[bytes/sizeof(wchar_t)-1]!=0) return L"";
    return text;
}
static std::wstring FindPython() {
    // PEP 514 registrations work even when a GUI process has an older PATH.
    std::wstring found; int newest=9;
    HKEY roots[]={HKEY_CURRENT_USER,HKEY_LOCAL_MACHINE};
    REGSAM views[]={KEY_WOW64_64KEY,KEY_WOW64_32KEY};
    for(unsigned r=0;r<2;++r) for(unsigned v=0;v<2;++v) {
        HKEY company;
        if(RegOpenKeyExW(roots[r],L"Software\\Python\\PythonCore",0,KEY_READ|views[v],&company)!=ERROR_SUCCESS) continue;
        for(DWORD i=0;;++i) {
            wchar_t tag[256]; DWORD count=256;
            LONG result=RegEnumKeyExW(company,i,tag,&count,NULL,NULL,NULL,NULL);
            if(result==ERROR_NO_MORE_ITEMS) break;
            if(result!=ERROR_SUCCESS) continue;
            int major=0,minor=0;
            if(swscanf(tag,L"%d.%d",&major,&minor)!=2 || major!=3 || minor<=newest) continue;
            HKEY install;
            if(RegOpenKeyExW(company,(std::wstring(tag)+L"\\InstallPath").c_str(),0,KEY_READ|views[v],&install)!=ERROR_SUCCESS) continue;
            std::wstring executable=RegistryText(install,L"ExecutablePath");
            if(executable.empty()) {
                executable=RegistryText(install,NULL);
                if(!executable.empty()) executable+=executable[executable.size()-1]==L'\\'?L"python.exe":L"\\python.exe";
            }
            RegCloseKey(install);
            DWORD attributes=GetFileAttributesW(executable.c_str());
            if(!executable.empty() && attributes!=INVALID_FILE_ATTRIBUTES && !(attributes&FILE_ATTRIBUTE_DIRECTORY)) {
                newest=minor; found=executable;
            }
        }
        RegCloseKey(company);
    }
    if(found.empty()) {
        wchar_t path[32768];
        DWORD length=GetEnvironmentVariableW(L"PATH",path,32768);
        if(length && length<32768) found=FindPythonOnPath(path);
    }
    return found;
}
__fastcall TReportWorker::TReportWorker(const std::wstring& scriptPath,const std::wstring& folder,const std::string& iso,__int64 periodStart)
 : TThread(true),script(scriptPath),history(folder),output(folder+L"\\reports"),since(periodStart),Error(0) {
    FreeOnTerminate=false;
    for(unsigned i=0;i<iso.size();++i) region+=(wchar_t)iso[i];
    Report=output+L"\\history.html";
}
void __fastcall TReportWorker::Execute() {
    try {
        std::wstring executable=FindPython();
        if(executable.empty()) { Error=ERROR_FILE_NOT_FOUND; return; }
        std::wstring command=std::wstring(L"\"")+executable+L"\" -I \""+script+L"\" --history \""+history+
            L"\" --output \""+output+L"\" --region \""+region+L"\"";
        if(since>0) { wchar_t number[40]; swprintf(number,L"%I64d",since); command+=std::wstring(L" --since ")+number; }
        std::vector<wchar_t> environment;
        wchar_t* inherited=GetEnvironmentStringsW();
        if(!inherited) { Error=GetLastError(); return; }
        const wchar_t* excluded[]={L"UKRAINEALARM_API_KEY=",L"ALERTS_IN_UA_TOKEN="};
        for(const wchar_t* item=inherited;*item;item+=wcslen(item)+1) {
            bool keep=true;
            for(unsigned i=0;i<2;++i) if(_wcsnicmp(item,excluded[i],wcslen(excluded[i]))==0) keep=false;
            if(keep) environment.insert(environment.end(),item,item+wcslen(item)+1);
        }
        FreeEnvironmentStringsW(inherited); environment.push_back(0);
        if(environment.size()==1) environment.push_back(0);
        STARTUPINFOW startup={0}; startup.cb=sizeof(startup);
        PROCESS_INFORMATION process={0};
        if(!CreateProcessW(executable.c_str(),&command[0],NULL,NULL,FALSE,CREATE_NO_WINDOW|CREATE_UNICODE_ENVIRONMENT,&environment[0],NULL,&startup,&process)) {
            Error=GetLastError(); return;
        }
        DWORD wait=WaitForSingleObject(process.hProcess,120000),code=1;
        if(wait==WAIT_OBJECT_0) GetExitCodeProcess(process.hProcess,&code);
        else { TerminateProcess(process.hProcess,1); code=ERROR_TIMEOUT; }
        CloseHandle(process.hThread); CloseHandle(process.hProcess);
        Error=code;
    } catch(...) { Error=ERROR_GEN_FAILURE; }
}
