#include <vcl.h>
#include <stdio.h>
#include "../src/HistoryReport.h"
int main() {
    wchar_t temp[MAX_PATH],suffix[80],executable[32768],oldPath[32768];
    GetTempPathW(MAX_PATH,temp);
    wsprintfW(suffix,L"AirAlertMap Report Tests %lu-%lu",GetCurrentProcessId(),GetTickCount());
    std::wstring root=std::wstring(temp)+suffix;
    CreateDirectoryW(root.c_str(),NULL);
    GetModuleFileNameW(NULL,executable,32768);
    std::wstring script=executable; script=script.substr(0,script.find_last_of(L"\\/"))+L"\\analytics\\analyze.py";
    DWORD oldLength=GetEnvironmentVariableW(L"PATH",oldPath,32768);
    SetEnvironmentVariableW(L"PATH",L"");
    TReportWorker* worker=new TReportWorker(script,root,"UA-30");
    worker->Start(); worker->WaitFor();
    unsigned long error=worker->Error;
    bool exists=GetFileAttributesW(worker->Report.c_str())!=INVALID_FILE_ATTRIBUTES;
    delete worker;
    // Synthetic values only: the child must not inherit API-key environment variables.
    SetEnvironmentVariableW(L"UKRAINEALARM_API_KEY",L"AirAlertMap-test-sentinel-one");
    SetEnvironmentVariableW(L"ALERTS_IN_UA_TOKEN",L"AirAlertMap-test-sentinel-two");
    std::wstring probe=root+L"\\environment_probe.py";
    FILE* source=_wfopen(probe.c_str(),L"wb");
    if(source) {
        const char* code="import os\nassert 'UKRAINEALARM_API_KEY' not in os.environ\nassert 'ALERTS_IN_UA_TOKEN' not in os.environ\n";
        fwrite(code,1,strlen(code),source); fclose(source);
    }
    TReportWorker* isolated=new TReportWorker(probe,root,"UA-30");
    isolated->Start(); isolated->WaitFor();
    unsigned long environmentError=isolated->Error;
    delete isolated; DeleteFileW(probe.c_str());
    SetEnvironmentVariableW(L"UKRAINEALARM_API_KEY",NULL);
    SetEnvironmentVariableW(L"ALERTS_IN_UA_TOKEN",NULL);
    wchar_t workingDirectory[32768]; GetCurrentDirectoryW(32768,workingDirectory);
    FILE* fake=_wfopen((root+L"\\python.exe").c_str(),L"wb"); if(fake) fclose(fake);
    SetCurrentDirectoryW(root.c_str());
    bool ignoresCurrentDirectory=FindPythonOnPath(L"").empty() && FindPythonOnPath(L".").empty();
    bool acceptsExplicitPath=FindPythonOnPath(root)==root+L"\\python.exe";
    SetCurrentDirectoryW(workingDirectory); DeleteFileW((root+L"\\python.exe").c_str());
    SetEnvironmentVariableW(L"PATH",oldLength?oldPath:NULL);
    const wchar_t* names[]={L"history.html",L"history.csv",L"history.json"};
    for(unsigned i=0;i<3;++i) DeleteFileW((root+L"\\reports\\"+names[i]).c_str());
    RemoveDirectoryW((root+L"\\reports").c_str()); RemoveDirectoryW(root.c_str());
    if(error || !exists) { fprintf(stderr,"FAIL: GUI report creation without PATH: error %lu\n",error); return 1; }
    if(environmentError) { fprintf(stderr,"FAIL: Python must not inherit API keys: error %lu\n",environmentError); return 1; }
    if(!ignoresCurrentDirectory || !acceptsExplicitPath) { fprintf(stderr,"FAIL: Python search must use explicit absolute PATH entries\n"); return 1; }
    puts("PASS: Python report launch, spaced paths, API-key isolation, absolute PATH search");
    return 0;
}
