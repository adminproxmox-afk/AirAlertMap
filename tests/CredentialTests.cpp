#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincred.h>
#include "../src/ApiCredentials.h"
#include <stdio.h>
#include <string.h>
#pragma link "advapi32.lib"

static int checks=0;
static void check(bool ok,const char* name) {
    ++checks;
    if(!ok) { fprintf(stderr,"FAIL: %s\n",name); throw 1; }
}
static bool ChildReadsCredential(const std::wstring& target) {
    wchar_t executable[MAX_PATH];
    if(!GetModuleFileNameW(NULL,executable,MAX_PATH)) return false;
    std::wstring command=std::wstring(L"\"")+executable+L"\" --read \""+target+L"\"";
    STARTUPINFOW startup={0}; startup.cb=sizeof(startup);
    PROCESS_INFORMATION process={0};
    if(!CreateProcessW(NULL,&command[0],NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startup,&process)) return false;
    DWORD wait=WaitForSingleObject(process.hProcess,10000),exitCode=1;
    if(wait==WAIT_OBJECT_0) GetExitCodeProcess(process.hProcess,&exitCode);
    else TerminateProcess(process.hProcess,1);
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
    return wait==WAIT_OBJECT_0 && exitCode==0;
}
static void RunTests(const std::wstring& target) {
    const wchar_t* name=target.c_str();
    std::wstring restored=L"unchanged"; ApiProvider source=apAlertsInUa;
    check(!LoadApiCredential(name,restored,source),"missing credential is normal");
    check(restored==L"unchanged" && source==apAlertsInUa,"missing entry never changes current settings");
    check(SaveApiCredential(name,L"unit-test-key-one",apUkraineAlarm),"save Windows credential");
    check(LoadApiCredential(name,restored,source) && restored==L"unit-test-key-one" && source==apUkraineAlarm,
          "restore key together with its provider");
    check(ChildReadsCredential(target),"new process restores the saved credential");
    PCREDENTIALW entry=NULL;
    check(CredReadW(name,CRED_TYPE_GENERIC,0,&entry)!=FALSE,"Windows owns the stored entry");
    bool persistent=entry->Persist==CRED_PERSIST_LOCAL_MACHINE;
    if(entry->CredentialBlob) SecureZeroMemory(entry->CredentialBlob,entry->CredentialBlobSize);
    CredFree(entry);
    check(persistent,"credential persists beyond the current application session");
    check(SaveApiCredential(name,L"unit-test-key-two",apAlertsInUa),"replace saved key and provider");
    check(LoadApiCredential(name,restored,source) && restored==L"unit-test-key-two" && source==apAlertsInUa,
          "updated provider is remembered");
    check(!SaveApiCredential(name,L"",apUkraineAlarm),"empty token is not saved");
    check(!SaveApiCredential(name,L"bad\r\nheader",apUkraineAlarm),"invalid token is not saved");
    check(!SaveApiCredential(name,L"unit-test-key",(ApiProvider)99),"unknown provider is not saved");
    check(LoadApiCredential(name,restored,source) && restored==L"unit-test-key-two",
          "failed save preserves the previous credential");
    std::wstring maximum(2048,L'K');
    check(SaveApiCredential(name,maximum,apUkraineAlarm),"maximum supported API key fits credential storage");
    check(LoadApiCredential(name,restored,source) && restored==maximum,"maximum supported key round-trip");
    check(DeleteApiCredential(name),"remove saved credential when remembering is disabled");
    check(!LoadApiCredential(name,restored,source),"removed credential is no longer loaded");
    check(DeleteApiCredential(name),"removing an absent credential succeeds");
    check(!SaveApiCredential(NULL,L"unit-test-key",apUkraineAlarm),"invalid storage target is rejected");
}
int main(int argc,char** argv) {
    if(argc==3 && strcmp(argv[1],"--read")==0) {
        std::wstring target;
        for(const char* c=argv[2];*c;++c) target+=(wchar_t)(unsigned char)*c;
        std::wstring token; ApiProvider provider=apAlertsInUa;
        return LoadApiCredential(target.c_str(),token,provider) &&
            token==L"unit-test-key-one" && provider==apUkraineAlarm?0:1;
    }
    wchar_t suffix[80]; wsprintfW(suffix,L"AirAlertMap.Tests/%lu-%lu",GetCurrentProcessId(),GetTickCount());
    std::wstring target=suffix;
    int result=0;
    try { RunTests(target); printf("PASS: %d Windows credential checks including restart\n",checks); }
    catch(...) { result=1; }
    DeleteApiCredential(target.c_str()); // Never touches the application's real entry.
    return result;
}
