#include "SingleInstance.h"
#include <wchar.h>

// Startup, binding and destruction all run on the application's UI thread.
static SingleInstanceGuard* guards=NULL;

struct WindowSearch {
    const wchar_t* property;
    HWND window;
    DWORD process;
};
static BOOL CALLBACK FindInstanceWindow(HWND window,LPARAM parameter) {
    WindowSearch* search=(WindowSearch*)parameter;
    DWORD process=0;
    GetWindowThreadProcessId(window,&process);
    // Both identity and owner PID must match. Titles are never consulted.
    if(process && GetPropW(window,search->property)==(HANDLE)(ULONG_PTR)process) {
        search->window=window;
        search->process=process;
        return FALSE;
    }
    return TRUE;
}
static DWORD LastErrorOr(DWORD fallback) {
    DWORD error=GetLastError();
    return error?error:fallback;
}
SingleInstanceGuard::SingleInstanceGuard(const wchar_t* identity)
    : mutex_(NULL),first_(false),error_(0),message_(0),window_(NULL),previous_(guards) {
    guards=this;
    if(!identity || !*identity || wcslen(identity)>200) {
        error_=ERROR_INVALID_PARAMETER;
        return;
    }
    identity_=identity;
    property_=identity_+L".Window";
    message_=RegisterWindowMessageW((identity_+L".Activate").c_str());
    if(!message_) { error_=LastErrorOr(ERROR_NOT_ENOUGH_MEMORY); return; }
    SetLastError(ERROR_SUCCESS);
    mutex_=CreateMutexW(NULL,TRUE,identity_.c_str());
    if(!mutex_) { error_=LastErrorOr(ERROR_INVALID_HANDLE); return; }
    if(GetLastError()!=ERROR_ALREADY_EXISTS) { first_=true; return; }
    // Win32 mutexes are recursive on one thread; a second local guard must not
    // mistake that recursion for new ownership.
    for(SingleInstanceGuard* guard=previous_;guard;guard=guard->previous_)
        if(guard->first_ && guard->identity_==identity_) return;
    DWORD wait=WaitForSingleObject(mutex_,0);
    if(wait==WAIT_OBJECT_0 || wait==WAIT_ABANDONED) first_=true;
    else if(wait!=WAIT_TIMEOUT) error_=LastErrorOr(ERROR_INVALID_HANDLE);
}
SingleInstanceGuard::~SingleInstanceGuard() {
    if(window_ && GetPropW(window_,property_.c_str())==(HANDLE)(ULONG_PTR)GetCurrentProcessId())
        RemovePropW(window_,property_.c_str());
    if(first_ && mutex_) ReleaseMutex(mutex_);
    if(mutex_) CloseHandle(mutex_);
    SingleInstanceGuard** link=&guards;
    while(*link && *link!=this) link=&(*link)->previous_;
    if(*link) *link=previous_;
}
bool SingleInstanceGuard::IsFirst() const { return first_; }
DWORD SingleInstanceGuard::Error() const { return error_; }
bool SingleInstanceGuard::ActivateExisting(DWORD timeoutMs) {
    if(first_) return false;
    if(!mutex_ || !message_) return false;
    // Even an accidentally unbounded caller cannot block startup indefinitely.
    if(timeoutMs>5000) timeoutMs=5000;
    error_=0;
    DWORD start=GetTickCount();
    do {
        bool localOwner=false;
        for(SingleInstanceGuard* guard=guards;guard;guard=guard->previous_)
            if(guard!=this && guard->first_ && guard->identity_==identity_) localOwner=true;
        if(!localOwner) {
            DWORD wait=WaitForSingleObject(mutex_,0);
            if(wait==WAIT_OBJECT_0 || wait==WAIT_ABANDONED) {
                first_=true;
                return false; // Caller rechecks IsFirst() and proceeds with startup.
            }
            if(wait!=WAIT_TIMEOUT) { error_=LastErrorOr(ERROR_INVALID_HANDLE); return false; }
        }
        WindowSearch search={property_.c_str(),NULL,0};
        EnumWindows(FindInstanceWindow,(LPARAM)&search);
        if(search.window) {
            DWORD elapsed=GetTickCount()-start;
            DWORD remaining=elapsed<timeoutMs?timeoutMs-elapsed:0;
            if(!remaining) break;
            // Grants foreground permission when this invocation was launched by
            // the user. Windows may still enforce its normal foreground rules.
            AllowSetForegroundWindow(search.process);
            DWORD_PTR result=0;
            SetLastError(ERROR_SUCCESS);
            if(SendMessageTimeoutW(search.window,message_,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,
                remaining,&result) && result==1) return true;
            DWORD error=LastErrorOr(ERROR_TIMEOUT);
            if(error!=ERROR_TIMEOUT && IsWindow(search.window)) { error_=error; return false; }
        }
        DWORD elapsed=GetTickCount()-start;
        if(elapsed>=timeoutMs) break;
        DWORD remaining=timeoutMs-elapsed;
        Sleep(remaining<25?remaining:25);
    } while(GetTickCount()-start<timeoutMs);
    error_=ERROR_TIMEOUT;
    return false;
}
void SingleInstanceGuard::AttachWindow(HWND window) {
    if(!first_) return;
    DWORD process=0;
    if(!window || !GetWindowThreadProcessId(window,&process) || process!=GetCurrentProcessId()) {
        error_=ERROR_INVALID_WINDOW_HANDLE;
        return;
    }
    SetLastError(ERROR_SUCCESS);
    if(!SetPropW(window,property_.c_str(),(HANDLE)(ULONG_PTR)process)) {
        error_=LastErrorOr(ERROR_NOT_ENOUGH_MEMORY);
        return;
    }
    if(window_ && window_!=window && GetPropW(window_,property_.c_str())==(HANDLE)(ULONG_PTR)process)
        RemovePropW(window_,property_.c_str());
    window_=window;
    error_=0;
}
void BindSingleInstanceWindow(HWND window) {
    for(SingleInstanceGuard* guard=guards;guard;guard=guard->previous_) {
        if(guard->first_) { guard->AttachWindow(window); return; }
    }
}
bool HandleSingleInstanceMessage(HWND window,UINT message) {
    for(SingleInstanceGuard* guard=guards;guard;guard=guard->previous_) {
        if(!guard->first_ || guard->window_!=window || message!=guard->message_ ||
            GetPropW(window,guard->property_.c_str())!=(HANDLE)(ULONG_PTR)GetCurrentProcessId()) continue;
        ShowWindow(window,IsIconic(window)?SW_RESTORE:SW_SHOW);
        BringWindowToTop(window);
        SetForegroundWindow(window);
        return true;
    }
    return false;
}
