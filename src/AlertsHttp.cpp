#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include "AlertsHttp.h"
#pragma link "winhttp.lib"

ApiResult::ApiResult() : error(aeNetwork),status(0),networkError(0),retrySeconds(60) {}

ApiResult DecodeApiResponse(unsigned status,const std::string& body,
                           const std::string& retryAfter,unsigned long ageSeconds) {
    ApiResult result;
    result.status=status;
    result.retrySeconds=RetryDelaySeconds(status,retryAfter);
    if(status==401 || status==403) result.error=aeToken;
    else if(status!=200) result.error=aeHttp;
    else if(ageSeconds>=90) result.error=aeStale;
    else if(!ParseOblastResponse(body,result.snapshot)) result.error=aeFormat;
    else result.error=aeNone;
    return result;
}

class HttpHandle {
    HINTERNET handle;
    HttpHandle(const HttpHandle&);
    HttpHandle& operator=(const HttpHandle&);
public:
    explicit HttpHandle(HINTERNET value) : handle(value) {}
    ~HttpHandle() { if(handle) WinHttpCloseHandle(handle); }
    operator HINTERNET() const { return handle; }
};

static std::wstring Header(HINTERNET request,DWORD name) {
    wchar_t buffer[512]; DWORD size=sizeof(buffer);
    if(!WinHttpQueryHeaders(request,name,WINHTTP_HEADER_NAME_BY_INDEX,buffer,&size,WINHTTP_NO_HEADER_INDEX)) return L"";
    return std::wstring(buffer);
}
static std::string Ascii(const std::wstring& value) {
    std::string result;
    for(unsigned i=0;i<value.size();++i) {
        if(value[i]>127) return "";
        result+=(char)value[i];
    }
    return result;
}
static std::string RetryHeader(HINTERNET request) {
    std::wstring value=Header(request,WINHTTP_QUERY_RETRY_AFTER);
    if(value.empty()) return "";
    // Retry-After supports both seconds and an HTTP date.
    SYSTEMTIME time; FILETIME future,now;
    if(WinHttpTimeToSystemTime(value.c_str(),&time) && SystemTimeToFileTime(&time,&future)) {
        GetSystemTimeAsFileTime(&now);
        ULARGE_INTEGER a,b; a.LowPart=future.dwLowDateTime; a.HighPart=future.dwHighDateTime;
        b.LowPart=now.dwLowDateTime; b.HighPart=now.dwHighDateTime;
        unsigned long seconds=a.QuadPart>b.QuadPart?(unsigned long)((a.QuadPart-b.QuadPart)/10000000):60;
        char text[32]; wsprintfA(text,"%lu",seconds); return text;
    }
    return Ascii(value);
}
static unsigned long AgeHeader(HINTERNET request) {
    std::string age=Ascii(Header(request,WINHTTP_QUERY_AGE));
    unsigned long seconds=0;
    for(unsigned i=0;i<age.size();++i) {
        if(age[i]<'0'||age[i]>'9') return 90; // Invalid cache age is not evidence of freshness.
        seconds=seconds*10+age[i]-'0';
        if(seconds>=90) return 90;
    }
    return seconds;
}

ApiResult FetchAlerts(const std::wstring& token) {
    ApiResult result;
    if(!IsValidApiToken(token)) { result.error=aeToken; return result; }
    const DWORD started=GetTickCount();
    HttpHandle session(WinHttpOpen(L"AirAlertMap/0.2",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                  WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    if(!session) { result.networkError=GetLastError(); return result; }
    if(!WinHttpSetTimeouts(session,3000,5000,5000,5000)) {
        result.networkError=GetLastError(); return result;
    }
    DWORD retries=1;
    if(!WinHttpSetOption(session,WINHTTP_OPTION_CONNECT_RETRIES,&retries,sizeof(retries))) {
        result.networkError=GetLastError(); return result;
    }
    HttpHandle connection(WinHttpConnect(session,L"api.alerts.in.ua",INTERNET_DEFAULT_HTTPS_PORT,0));
    if(!connection) { result.networkError=GetLastError(); return result; }
    HttpHandle request(WinHttpOpenRequest(connection,L"GET",L"/v2/iot/active_air_raid_alerts_by_oblast.json",
                                         NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
    if(!request) { result.networkError=GetLastError(); return result; }
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    DWORD logon=WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
    if(!WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect)) ||
       !WinHttpSetOption(request,WINHTTP_OPTION_AUTOLOGON_POLICY,&logon,sizeof(logon))) {
        result.networkError=GetLastError(); return result;
    }
    // Certificate validation stays at Windows defaults. No token in URL or logs.
    std::wstring headers=L"Authorization: Bearer "+token+L"\r\nAccept: application/json\r\nCache-Control: no-cache\r\n";
    if(!WinHttpSendRequest(request,headers.c_str(),headers.size(),WINHTTP_NO_REQUEST_DATA,0,0,0) ||
       !WinHttpReceiveResponse(request,NULL)) {
        result.networkError=GetLastError(); return result;
    }
    DWORD status=0,size=sizeof(status);
    if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)) {
        result.networkError=GetLastError(); return result;
    }
    std::string retry=RetryHeader(request);
    if(status!=200) return DecodeApiResponse(status,"",retry,0);
    unsigned long age=AgeHeader(request);
    std::string body;
    for(;;) {
        if((DWORD)(GetTickCount()-started)>20000UL) { result.networkError=ERROR_WINHTTP_TIMEOUT; return result; }
        char buffer[512]; DWORD read=0;
        if(!WinHttpReadData(request,buffer,sizeof(buffer),&read)) { result.networkError=GetLastError(); return result; }
        if(!read) break;
        if(body.size()+read>4096) { result.error=aeFormat; return result; }
        body.append(buffer,read);
    }
    return DecodeApiResponse(status,body,retry,age);
}
