#include "../src/AlertsHttp.h"
#include <stdio.h>
#include <stdlib.h>

static void check(bool ok,const char* name) {
    if(!ok) { fprintf(stderr,"FAIL: %s\n",name); exit(1); }
}
int main(int argc,char** argv) {
    const char* valid="\"RNNNNNNNNNNNNNNNNNNNNNNNNNN\"";
    check(DecodeApiResponse(200,valid,"",0).error==aeNone,"valid HTTP response");
    check(DecodeApiResponse(200,valid,"",90).error==aeStale,"reject stale cached data");
    check(DecodeApiResponse(200,"<html>error</html>","",0).error==aeFormat,"reject invalid body");
    check(DecodeApiResponse(401,valid,"",0).error==aeToken,"unauthorized response cannot turn regions clear");
    check(DecodeApiResponse(403,valid,"",0).error==aeForbidden,"403 is a separate access restriction");
    check(DecodeApiResponse(429,valid,"180",0).retrySeconds==180,"rate limited response respects delay");
    check(DecodeApiResponse(503,valid,"120",0).error==aeHttp,"server error rejects statuses");
    check(DecodeApiResponse(304,valid,"",0).error==aeHttp,"unsolicited 304 is not current data");
    check(DecodeApiResponse(302,valid,"",0).error==aeHttp,"redirect response is not current data");
    check(FetchAlerts(L"bad\r\nheader").error==aeToken,"invalid token rejected before HTTP");
    check(ApiAuthorization(L"example-key",apUkraineAlarm)==L"Authorization: example-key\r\n","UkraineAlarm receives raw key");
    check(ApiAuthorization(L"example-key",apAlertsInUa)==L"Authorization: Bearer example-key\r\n","alerts.in.ua receives bearer token");
    check(ApiAuthorization(L"bad\r\nheader",apUkraineAlarm).empty(),"no header injection for either provider");
    check(DecodeApiResponse(200,"[]","",0,apUkraineAlarm).error==aeFormat,"UkraineAlarm requires verified region coverage");
    check(DecodeApiResponse(200,valid,"",0,apUkraineAlarm).error==aeFormat,"never decode another provider's format");
    const std::string directory="{\"states\":[{\"regionId\":\"KC\",\"regionName\":\"\\u041a\\u0438\\u0457\\u0432\",\"regionChildIds\":[]}]}";
    ApiResult prepared=PrepareRegionDirectory(directory);
    check(prepared.error==aePreparing && prepared.regionDirectory==directory && prepared.snapshot.states[25]==asUnknown,
          "directory preparation does not publish a false all-clear");
    check(PrepareRegionDirectory("{}").error==aeFormat,"invalid directory cannot become a cache");
    puts("PASS: 17 HTTP response and provider checks");
    if(argc>1) {
        // Explicit smoke test: known fake token, never reads a real credential.
        ApiProvider source=std::string(argv[1])=="--smoke-ukrainealarm"?apUkraineAlarm:apAlertsInUa;
        ApiResult live=FetchAlerts(L"AirAlertMap-smoke-test-invalid-token",source);
        printf("HTTPS smoke: HTTP %u, WinHTTP error %lu\n",live.status,live.networkError);
        check((live.error==aeToken && live.status==401) ||
              (live.error==aeForbidden && live.status==403),"real HTTPS access failure handled");
        puts("PASS: real HTTPS authentication failure handled");
    }
    return 0;
}
