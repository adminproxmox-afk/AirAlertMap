#include "../src/AlertFeed.h"
#include <stdio.h>
#include <stdlib.h>
#include <string>

static int checks = 0;
static void check(bool ok, const char* name) {
    ++checks;
    if (!ok) { fprintf(stderr,"FAIL: %s\n",name); exit(1); }
}

int main() {
    AlertSnapshot snapshot;
    check(!ParseOblastResponse("{}",snapshot),"reject an API error object");
    check(!ParseOblastResponse("\"NN\"",snapshot),"reject truncated response");
    check(!ParseOblastResponse("\"NNNNNNNNNNNNNNNNNNNNNNNNNNNN\"",snapshot),"reject excess regions");
    check(!ParseOblastResponse("\"NNNNNNNNNNNNNNNNNNNNNNNNNN?\"",snapshot),"reject unrecognized code");
    check(!ParseOblastResponse("\"NNNNNNNNNNNNNNNNNNNNNNNNNNN\" trailing",snapshot),"reject trailing data");
    check(ParseOblastResponse(" \r\n\"NNNNNNNNNNNNNNNNNNNNNNNNNNN\"\t",snapshot),"accept JSON whitespace");
    check(snapshot.states[0]==asClear && snapshot.levels[26]=='N',"no-alert response maps to clear");

    // Independent table: provider order -> our map geometry order.
    const int want[27]={18,1,19,23,9,3,13,10,12,25,4,24,8,11,17,16,22,2,26,6,14,7,0,20,21,15,5};
    for(int source=0;source<27;++source) {
        std::string body(27,'N'); body[source]='R';
        check(ParseOblastResponse("\""+body+"\"",snapshot),"parse single-region fixture");
        check(snapshot.states[want[source]]==asAlert && snapshot.levels[want[source]]=='R',"correct region identity");
        int active=0;
        for(int i=0;i<27;++i) if(snapshot.states[i]==asAlert) ++active;
        check(active==1,"mapping does not duplicate alerts");
    }
    check(ParseOblastResponse("\"YPYNNNNNNNNNNNNNNNNNNNNNNNN\"",snapshot),"accept partial and yellow");
    check(snapshot.levels[18]=='Y' && snapshot.levels[1]=='P' && snapshot.states[1]==asAlert,
          "preserve severity and count partial as active");
    check(!ParseOblastResponse("\"NNNNNNNNNNNNNNNNNNNNNNNNNNX\"",snapshot),"malformed update fails");
    check(snapshot.levels[18]=='Y',"failed parse never partially changes previous snapshot");
    check(ParseOblastResponse("\"\\u0052NNNNNNNNNNNNNNNNNNNNNNNNNN\"",snapshot),"accept escaped JSON status");
    check(snapshot.levels[18]=='R',"decode escaped JSON status");
    check(!ParseOblastResponse("\"\\u00ZZNNNNNNNNNNNNNNNNNNNNNNNNNN\"",snapshot),"reject malformed JSON escape");
    check(IsValidApiToken(L"token-123_ABC.xyz"),"allow bearer token");
    check(!IsValidApiToken(L"") && !IsValidApiToken(L"abc\r\nInjected:x"),"reject missing token and header injection");
    check(!IsValidApiToken(L"a b") && !IsValidApiToken(std::wstring(2049,L'x')),"reject whitespace and excessive token size");
    check(RetryDelaySeconds(429,"120")==120,"respect Retry-After seconds");
    check(RetryDelaySeconds(429,"1")==60,"rate limit minimum delay");
    check(RetryDelaySeconds(429,"9999999999999999999999")==3600,"bound numeric overflow");
    check(RetryDelaySeconds(429,"Tue, 03 Nov 2026 09:00:00 GMT")==60,"date header falls back to safe minimum");
    check(RetryDelaySeconds(503,"")==60,"normal retry delay");
    AlertFeed feed;
    check(!feed.available && feed.snapshot.states[0]==asUnknown,"start without fictional data");
    feed.Accept(snapshot,1000);
    check(feed.available && feed.snapshot.states[18]==asAlert,"publish validated snapshot");
    check(!feed.Expire(90999),"confirmed data remains valid below 90 seconds");
    check(feed.Expire(91000) && !feed.available,"expire at 90 seconds");
    check(feed.snapshot.states[18]==asUnknown && feed.snapshot.levels[18]=='?',"expired data is unknown");
    feed.Accept(snapshot,0xfffffff0UL);
    check(!feed.Expire(0x20UL),"monotonic clock wrap does not expire fresh data");
    feed.Unavailable();
    check(!feed.available && feed.snapshot.states[0]==asUnknown,"network failure clears previous status");
    printf("PASS: %d feed checks including all 27 region mappings\n",checks);
    return 0;
}
