#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include "AlertsHttp.h"
#include "UkraineAlarmFeed.h"
#pragma link "winhttp.lib"

ApiResult::ApiResult() : error(aeNetwork),status(0),networkError(0),retrySeconds(60) {}

ApiResult PrepareRegionDirectory(const std::string& directory) {
    ApiResult result;
    AlertSnapshot sanity;
    result.status=200;
    if(!ParseUkraineAlarmCoverage("[]",directory,sanity)) result.error=aeFormat;
    else { result.regionDirectory=directory; result.error=aePreparing; }
    return result;
}

const wchar_t* ApiProviderName(ApiProvider provider) {
    return provider==apUkraineAlarm?L"UkraineAlarm / Stfalcon":L"alerts.in.ua";
}
std::wstring ApiAuthorization(const std::wstring& token,ApiProvider provider) {
    if(!IsValidApiToken(token)) return L"";
    return std::wstring(L"Authorization: ")+(provider==apUkraineAlarm?L"":L"Bearer ")+token+L"\r\n";
}

ApiResult DecodeApiResponse(unsigned status,const std::string& body,
                           const std::string& retryAfter,unsigned long ageSeconds,ApiProvider provider,const std::string& directory) {
    ApiResult result;
    result.status=status;
    result.retrySeconds=RetryDelaySeconds(status,retryAfter);
    if(status==401) result.error=aeToken;
    else if(status==403) result.error=aeForbidden;
    else if(status!=200) result.error=aeHttp;
    else if(ageSeconds>=90) result.error=aeStale;
    else if(!(provider==apUkraineAlarm?ParseUkraineAlarmCoverage(body,directory,result.snapshot):ParseOblastResponse(body,result.snapshot))) result.error=aeFormat;
    else result.error=aeNone;
    if(result.error==aeNone && provider==apUkraineAlarm) result.regionDirectory=directory;
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

static void SavePublicDiagnostic(const wchar_t* path,const std::string& body) {
    wchar_t enabled[4];
    if(GetEnvironmentVariableW(L"AIRALERT_DIAGNOSTICS",enabled,4)!=1 || enabled[0]!=L'1') return;
    std::string clean=PublicUkraineAlarmDiagnostic(body);
    if(clean.empty()) return;
    wchar_t executable[32768];
    DWORD length=GetModuleFileNameW(NULL,executable,32768);
    if(!length || length>=32768) return;
    std::wstring folder(executable,length);
    folder=folder.substr(0,folder.find_last_of(L"\\/"))+L"\\diagnostics";
    CreateDirectoryW(folder.c_str(),NULL);
    std::wstring file=folder+(std::wstring(path)==L"/api/v3/regions"?L"\\regions.json":L"\\alerts.json");
    FILE* output=_wfopen(file.c_str(),L"wb");
    if(output) { fwrite(clean.data(),1,clean.size(),output); fclose(output); }
}

static ApiResult GetJson(HINTERNET connection,const wchar_t* path,const std::wstring& token,
                         ApiProvider provider,DWORD started,std::string& body) {
    ApiResult result;
    HttpHandle request(WinHttpOpenRequest(connection,L"GET",path,
                                         NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
    if(!request) { result.networkError=GetLastError(); return result; }
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    DWORD logon=WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
    if(!WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect)) ||
       !WinHttpSetOption(request,WINHTTP_OPTION_AUTOLOGON_POLICY,&logon,sizeof(logon))) {
        result.networkError=GetLastError(); return result;
    }
    // Certificate validation stays at Windows defaults. No token in URL or logs.
    std::wstring headers=ApiAuthorization(token,provider)+L"Accept: application/json\r\nCache-Control: no-cache\r\n";
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
    result.status=status;
    result.retrySeconds=RetryDelaySeconds(status,retry);
    if(status!=200) return DecodeApiResponse(status,"",retry,0,provider);
    unsigned long age=AgeHeader(request);
    if(age>=90) { result.error=aeStale; return result; }
    for(;;) {
        if((DWORD)(GetTickCount()-started)>30000UL) { result.networkError=ERROR_WINHTTP_TIMEOUT; return result; }
        char buffer[512]; DWORD read=0;
        if(!WinHttpReadData(request,buffer,sizeof(buffer),&read)) { result.networkError=GetLastError(); return result; }
        if(!read) break;
        unsigned maxBytes=provider==apUkraineAlarm?2*1024*1024:4096;
        if(body.size()+read>maxBytes) { result.error=aeFormat; return result; }
        body.append(buffer,read);
    }
    result.error=aeNone;
    if(provider==apUkraineAlarm) SavePublicDiagnostic(path,body);
    return result;
}

ApiResult FetchAlerts(const std::wstring& token,ApiProvider provider,const std::string& cachedDirectory) {
    ApiResult result;
    if(!IsValidApiToken(token)) { result.error=aeToken; return result; }
    const DWORD started=GetTickCount();
    HttpHandle session(WinHttpOpen(L"AirAlertMap/0.3",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                  WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    if(!session) { result.networkError=GetLastError(); return result; }
    if(!WinHttpSetTimeouts(session,3000,5000,5000,5000)) {
        result.networkError=GetLastError(); return result;
    }
    DWORD retries=1;
    if(!WinHttpSetOption(session,WINHTTP_OPTION_CONNECT_RETRIES,&retries,sizeof(retries))) {
        result.networkError=GetLastError(); return result;
    }
    const wchar_t* host=provider==apUkraineAlarm?L"api.ukrainealarm.com":L"api.alerts.in.ua";
    HttpHandle connection(WinHttpConnect(session,host,INTERNET_DEFAULT_HTTPS_PORT,0));
    if(!connection) { result.networkError=GetLastError(); return result; }
    std::string directory=cachedDirectory;
    if(provider==apUkraineAlarm) {
        AlertSnapshot sanity;
        if(directory.empty() || !ParseUkraineAlarmCoverage("[]",directory,sanity)) {
            directory.clear();
            result=GetJson(connection,L"/api/v3/regions",token,provider,started,directory);
            if(result.error!=aeNone) return result;
            // Do not burst two authenticated requests on initial connection.
            // Publish only the directory now; the next scheduled request fetches alerts.
            return PrepareRegionDirectory(directory);
        }
    }
    std::string body;
    const wchar_t* path=provider==apUkraineAlarm?L"/api/v3/alerts":L"/v2/iot/active_air_raid_alerts_by_oblast.json";
    result=GetJson(connection,path,token,provider,started,body);
    if(result.error!=aeNone) { result.regionDirectory=directory; return result; }
    return DecodeApiResponse(200,body,"",0,provider,directory);
}
