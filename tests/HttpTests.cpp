#include "../src/AlertsHttp.h"
#include <stdio.h>
#include <stdlib.h>

static void check(bool ok,const char* name) {
    if(!ok) { fprintf(stderr,"FAIL: %s\n",name); exit(1); }
}
int main(int argc,char**) {
    const char* valid="\"RNNNNNNNNNNNNNNNNNNNNNNNNNN\"";
    check(DecodeApiResponse(200,valid,"",0).error==aeNone,"valid HTTP response");
    check(DecodeApiResponse(200,valid,"",90).error==aeStale,"reject stale cached data");
    check(DecodeApiResponse(200,"<html>error</html>","",0).error==aeFormat,"reject invalid body");
    check(DecodeApiResponse(401,valid,"",0).error==aeToken,"unauthorized response cannot turn regions clear");
    check(DecodeApiResponse(403,valid,"",0).error==aeToken,"forbidden response suspends retries");
    check(DecodeApiResponse(429,valid,"180",0).retrySeconds==180,"rate limited response respects delay");
    check(DecodeApiResponse(503,valid,"120",0).error==aeHttp,"server error rejects statuses");
    check(DecodeApiResponse(304,valid,"",0).error==aeHttp,"unsolicited 304 is not current data");
    check(DecodeApiResponse(302,valid,"",0).error==aeHttp,"redirect response is not current data");
    check(FetchAlerts(L"bad\r\nheader").error==aeToken,"invalid token rejected before HTTP");
    puts("PASS: 10 HTTP response checks");
    if(argc>1) {
        // Explicit smoke test: known fake token, never reads a real credential.
        ApiResult live=FetchAlerts(L"AirAlertMap-smoke-test-invalid-token");
        printf("HTTPS smoke: HTTP %u, WinHTTP error %lu\n",live.status,live.networkError);
        check(live.error==aeToken && (live.status==401||live.status==403),"real HTTPS unauthorized response handled");
        puts("PASS: real HTTPS authentication failure handled");
    }
    return 0;
}
