#include "../src/SingleInstance.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int checks=0;
static void check(bool ok,const char* name) {
    ++checks;
    if(!ok) { fprintf(stderr,"FAIL: %s\n",name); throw 1; }
}
static std::wstring Wide(const char* value) {
    std::wstring result;
    while(*value) result+=(wchar_t)(unsigned char)*value++;
    return result;
}
static HANDLE OpenSignal(const std::wstring& id,const wchar_t* suffix) {
    return OpenEventW(EVENT_MODIFY_STATE|SYNCHRONIZE,FALSE,(id+suffix).c_str());
}
static HANDLE activated=NULL;
static HWND TestWindow(const wchar_t* name,WNDPROC procedure) {
    WNDCLASSW cls={0}; cls.lpfnWndProc=procedure; cls.hInstance=GetModuleHandleW(NULL); cls.lpszClassName=name;
    RegisterClassW(&cls);
    return CreateWindowExW(WS_EX_TOOLWINDOW,name,L"AirAlertMap",WS_OVERLAPPEDWINDOW,
        -30000,-30000,100,100,NULL,NULL,cls.hInstance,NULL);
}
static LRESULT CALLBACK WindowProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam) {
    if(HandleSingleInstanceMessage(window,message)) {
        if(!IsIconic(window)) SetEvent(activated);
        return 1;
    }
    return DefWindowProcW(window,message,wParam,lParam);
}
static int Owner(const std::wstring& id,DWORD delay) {
    SingleInstanceGuard guard(id.c_str());
    if(!guard.IsFirst() || guard.Error()) return 10;
    HANDLE ready=OpenSignal(id,L".Ready"),windowReady=OpenSignal(id,L".WindowReady");
    HANDLE stop=OpenSignal(id,L".Stop");
    activated=OpenSignal(id,L".Activated");
    if(!ready || !windowReady || !stop || !activated) return 11;
    SetEvent(ready);
    if(WaitForSingleObject(stop,delay)==WAIT_OBJECT_0) return 0;
    WNDCLASSW cls={0}; cls.lpfnWndProc=WindowProc; cls.hInstance=GetModuleHandleW(NULL); cls.lpszClassName=L"AirAlertMap.SingleInstanceTest";
    if(!RegisterClassW(&cls)) return 12;
    // A dedicated test HWND. Tool window stays off taskbar and off screen.
    HWND window=CreateWindowExW(WS_EX_TOOLWINDOW,cls.lpszClassName,L"AirAlertMap",WS_OVERLAPPEDWINDOW,
        -30000,-30000,100,100,NULL,NULL,cls.hInstance,NULL);
    if(!window) return 13;
    BindSingleInstanceWindow(window);
    if(guard.Error()) return 14;
    ShowWindow(window,SW_SHOWMINNOACTIVE);
    if(!IsIconic(window)) return 15;
    SetEvent(windowReady);
    DWORD start=GetTickCount(); bool done=false;
    while(!done && GetTickCount()-start<10000) {
        if(MsgWaitForMultipleObjects(1,&stop,FALSE,100,QS_ALLINPUT)==WAIT_OBJECT_0) done=true;
        MSG msg;
        while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    DestroyWindow(window);
    CloseHandle(ready); CloseHandle(windowReady); CloseHandle(stop); CloseHandle(activated);
    return 0;
}
struct Child {
    PROCESS_INFORMATION process;
    HANDLE ready,windowReady,activatedEvent,stop;
    Child() : ready(NULL),windowReady(NULL),activatedEvent(NULL),stop(NULL) { ZeroMemory(&process,sizeof(process)); }
    ~Child() {
        if(stop) SetEvent(stop);
        if(process.hProcess) {
            if(WaitForSingleObject(process.hProcess,3000)!=WAIT_OBJECT_0) { TerminateProcess(process.hProcess,99); WaitForSingleObject(process.hProcess,3000); }
            CloseHandle(process.hThread); CloseHandle(process.hProcess);
        }
        if(ready) CloseHandle(ready); if(windowReady) CloseHandle(windowReady);
        if(activatedEvent) CloseHandle(activatedEvent); if(stop) CloseHandle(stop);
    }
    bool Start(const std::wstring& id,const wchar_t* mode,DWORD value) {
        wchar_t executable[MAX_PATH],number[40];
        if(!GetModuleFileNameW(NULL,executable,MAX_PATH)) return false;
        wsprintfW(number,L" %lu",value);
        std::wstring command=std::wstring(L"\"")+executable+L"\" "+mode+L" \""+id+L"\""+number;
        STARTUPINFOW startup={0}; startup.cb=sizeof(startup);
        return CreateProcessW(NULL,&command[0],NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startup,&process)!=FALSE;
    }
    bool StartOwner(const std::wstring& id,DWORD delay) {
        ready=CreateEventW(NULL,TRUE,FALSE,(id+L".Ready").c_str());
        windowReady=CreateEventW(NULL,TRUE,FALSE,(id+L".WindowReady").c_str());
        activatedEvent=CreateEventW(NULL,TRUE,FALSE,(id+L".Activated").c_str());
        stop=CreateEventW(NULL,TRUE,FALSE,(id+L".Stop").c_str());
        return ready && windowReady && activatedEvent && stop && Start(id,L"--owner",delay) && WaitForSingleObject(ready,3000)==WAIT_OBJECT_0;
    }
    bool Exit(DWORD expected,DWORD timeout=6000) {
        DWORD code=999;
        return WaitForSingleObject(process.hProcess,timeout)==WAIT_OBJECT_0 && GetExitCodeProcess(process.hProcess,&code) && code==expected;
    }
};
static void RunTests(const std::wstring& base) {
    {
        SingleInstanceGuard first((base+L".Exclusive").c_str());
        check(first.IsFirst() && !first.Error(),"first process acquires identity before startup");
        Child second;
        check(second.Start(base+L".Exclusive",L"--duplicate",0) && second.Exit(0),"second process cannot own live first instance");
    }
    {
        SingleInstanceGuard reopened((base+L".Exclusive").c_str());
        check(reopened.IsFirst(),"normal exit releases identity");
    }
    {
        Child first;
        std::wstring id=base+L".Activate";
        HWND decoy=TestWindow(L"AirAlertMap.Decoy",DefWindowProcW);
        check(decoy!=NULL,"create unrelated window with matching application title");
        SetPropW(decoy,(id+L".Window").c_str(),(HANDLE)12345);
        check(first.StartOwner(id,0),"helper owns its unique identity");
        check(WaitForSingleObject(first.windowReady,3000)==WAIT_OBJECT_0,"first helper creates minimized test window");
        Child second;
        check(second.Start(id,L"--activate",2000) && second.Exit(0),"duplicate activates existing instance");
        check(WaitForSingleObject(first.activatedEvent,1000)==WAIT_OBJECT_0,"registered activation restores minimized first window");
        DestroyWindow(decoy);
    }
    {
        Child first;
        std::wstring id=base+L".Delayed";
        check(first.StartOwner(id,350),"delayed first helper has mutex before window");
        Child second;
        check(second.Start(id,L"--activate",2500) && second.Exit(0),"duplicate waits for startup window race");
        check(WaitForSingleObject(first.activatedEvent,1000)==WAIT_OBJECT_0,"startup race eventually restores owner window");
    }
    {
        Child first;
        std::wstring id=base+L".Timeout";
        check(first.StartOwner(id,2500),"timeout helper owns mutex without window");
        SingleInstanceGuard second(id.c_str());
        DWORD start=GetTickCount();
        check(!second.IsFirst() && !second.ActivateExisting(100) && second.Error()==ERROR_TIMEOUT,
            "missing owner window surfaces activation timeout");
        check(GetTickCount()-start<1500,"activation timeout remains bounded");
    }
    {
        Child first;
        std::wstring id=base+L".MaximumWait";
        check(first.StartOwner(id,7000),"five-second-bound helper owns mutex without window");
        SingleInstanceGuard second(id.c_str());
        DWORD start=GetTickCount();
        check(!second.ActivateExisting(INFINITE) && second.Error()==ERROR_TIMEOUT,
            "unbounded caller still receives startup timeout");
        check(GetTickCount()-start>=4500 && GetTickCount()-start<6000,"startup wait is capped at five seconds");
    }
    {
        Child first;
        std::wstring id=base+L".Crash";
        check(first.StartOwner(id,2500),"crash helper has acquired mutex");
        SingleInstanceGuard waiting(id.c_str()); // Retain abandoned kernel object.
        check(!waiting.IsFirst(),"waiting process cannot preempt owner");
        check(TerminateProcess(first.process.hProcess,77)!=FALSE && first.Exit(77),"terminate only dedicated helper process");
        check(!waiting.ActivateExisting(500) && waiting.IsFirst() && !waiting.Error(),"waiting duplicate takes over after owner crash");
        Child second;
        check(second.Start(id,L"--duplicate",0) && second.Exit(0),"recovered owner excludes additional processes");
    }
    {
        std::wstring id=base+L".Abandoned";
        Child first;
        check(first.StartOwner(id,2500),"abandoned helper owns mutex");
        HANDLE keep=OpenMutexW(SYNCHRONIZE,FALSE,id.c_str());
        check(keep!=NULL,"retain abandoned object for takeover test");
        check(TerminateProcess(first.process.hProcess,78)!=FALSE && first.Exit(78),"crash dedicated abandoned helper");
        SingleInstanceGuard next(id.c_str());
        CloseHandle(keep);
        check(next.IsFirst() && !next.Error(),"new process can acquire abandoned mutex");
    }
    {
        std::wstring id=base+L".Collision";
        HANDLE event=CreateEventW(NULL,FALSE,FALSE,id.c_str());
        SingleInstanceGuard invalid(id.c_str());
        bool rejected=!invalid.IsFirst() && invalid.Error()!=0;
        CloseHandle(event);
        check(rejected,"named object collision fails closed with error");
        SingleInstanceGuard empty(L"");
        check(!empty.IsFirst() && empty.Error()==ERROR_INVALID_PARAMETER,"empty identity rejected");
        check(!HandleSingleInstanceMessage(NULL,WM_USER),"unbound windows never consume unrelated messages");
    }
    {
        std::wstring id=base+L".Recreate",property=id+L".Window";
        UINT message=RegisterWindowMessageW((id+L".Activate").c_str());
        HWND oldWindow=TestWindow(L"AirAlertMap.Recreate",DefWindowProcW);
        HWND newWindow=TestWindow(L"AirAlertMap.Recreate",DefWindowProcW);
        check(oldWindow && newWindow,"create isolated window recreation fixtures");
        BindSingleInstanceWindow(oldWindow);
        check(GetPropW(oldWindow,property.c_str())==NULL,"binding without owned guard leaves window unadvertised");
        {
            SingleInstanceGuard guard(id.c_str());
            SingleInstanceGuard duplicate(id.c_str());
            check(!duplicate.IsFirst(),"same-thread recursive mutex cannot create second owner");
            BindSingleInstanceWindow(oldWindow);
            check(GetPropW(oldWindow,property.c_str())==(HANDLE)(ULONG_PTR)GetCurrentProcessId(),"bind property carries actual owner PID");
            BindSingleInstanceWindow(newWindow);
            check(!GetPropW(oldWindow,property.c_str()) && GetPropW(newWindow,property.c_str()),"window recreation moves advertised identity");
            check(!HandleSingleInstanceMessage(oldWindow,message),"replaced window no longer handles activation");
            check(!HandleSingleInstanceMessage(newWindow,WM_USER),"registered owner ignores unrelated message");
            guard.AttachWindow(NULL);
            check(guard.Error()==ERROR_INVALID_WINDOW_HANDLE,"invalid attachment surfaces error");
            guard.AttachWindow(newWindow);
            check(!guard.Error(),"successful rebind clears attachment error");
        }
        check(!GetPropW(newWindow,property.c_str()),"guard destruction removes published property");
        DestroyWindow(oldWindow); DestroyWindow(newWindow);
    }
}
int main(int argc,char** argv) {
    if(argc==4) {
        std::wstring id=Wide(argv[2]);
        DWORD value=(DWORD)strtoul(argv[3],NULL,10);
        if(strcmp(argv[1],"--owner")==0) return Owner(id,value);
        SingleInstanceGuard guard(id.c_str());
        if(strcmp(argv[1],"--duplicate")==0) return !guard.IsFirst() && !guard.Error()?0:20;
        if(strcmp(argv[1],"--activate")==0) return !guard.IsFirst() && guard.ActivateExisting(value) && !guard.Error()?0:21;
        return 22;
    }
    wchar_t name[120]; wsprintfW(name,L"Local\\AirAlertMap.Tests.%lu.%lu",GetCurrentProcessId(),GetTickCount());
    try { RunTests(name); printf("PASS: %d single-instance checks including helper activation/crash/startup races\n",checks); }
    catch(...) { return 1; }
    return 0;
}
