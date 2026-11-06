#ifndef SINGLE_INSTANCE_H
#define SINGLE_INSTANCE_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>

// Construct before VCL/API/storage startup; destroy on the same thread after Run().
class SingleInstanceGuard {
public:
    explicit SingleInstanceGuard(const wchar_t* identity=L"Local\\AirAlertMap.Instance.v1");
    ~SingleInstanceGuard();
    bool IsFirst() const;
    DWORD Error() const;
    // False can mean takeover: recheck IsFirst() before deciding to exit.
    bool ActivateExisting(DWORD timeoutMs=5000);
    void AttachWindow(HWND window);
private:
    SingleInstanceGuard(const SingleInstanceGuard&);
    SingleInstanceGuard& operator=(const SingleInstanceGuard&);
    HANDLE mutex_;
    bool first_;
    DWORD error_;
    std::wstring identity_;
    std::wstring property_;
    UINT message_;
    HWND window_;
    SingleInstanceGuard* previous_;
    friend void BindSingleInstanceWindow(HWND window);
    friend bool HandleSingleInstanceMessage(HWND window,UINT message);
};

// Call on each VCL CreateWnd. If Handle returns true, consume the message and
// set its result to 1 so the second invocation receives acknowledgement.
void BindSingleInstanceWindow(HWND window);
bool HandleSingleInstanceMessage(HWND window,UINT message);
#endif
