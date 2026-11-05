#include "AlertFeed.h"

AlertVisual VisualForState(unsigned char state,char level) {
    if(state==asUnknown) return avUnknown;
    if(state==asClear) return avClear;
    if(level=='P' || level=='Y') return avAmber;
    return avRed;
}

AlertSnapshot::AlertSnapshot() {
    for(int i=0;i<RegionCount;++i) { states[i]=asUnknown; levels[i]='?'; }
}
static bool whitespace(char c) { return c==' ' || c=='\t' || c=='\r' || c=='\n'; }
static int hex(char c) {
    if(c>='0'&&c<='9') return c-'0';
    if(c>='a'&&c<='f') return c-'a'+10;
    if(c>='A'&&c<='F') return c-'A'+10;
    return -1;
}
bool ParseOblastResponse(const std::string& json, AlertSnapshot& result) {
    // Contract: JSON string containing exactly 27 ASCII status codes.
    // Never expose a partial parse to the UI.
    if(json.size()>4096) return false;
    unsigned p=0;
    while(p<json.size() && whitespace(json[p])) ++p;
    if(p==json.size() || json[p++]!='"') return false;
    const int providerToMap[27]={18,1,19,23,9,3,13,10,12,25,4,24,8,11,17,16,22,2,26,6,14,7,0,20,21,15,5};
    AlertSnapshot parsed;
    for(int i=0;i<RegionCount;++i) {
        if(p>=json.size()) return false;
        char code=json[p++];
        if(code=='\\') {
            if(p>=json.size() || json[p++]!='u' || p+4>json.size()) return false;
            int value=0;
            for(int j=0;j<4;++j) {
                int digit=hex(json[p++]); if(digit<0) return false;
                value=value*16+digit;
            }
            if(value>127) return false;
            code=(char)value;
        }
        if(code!='N' && code!='R' && code!='Y' && code!='P') return false;
        int target=providerToMap[i];
        parsed.levels[target]=code;
        parsed.states[target]=(code=='N')?asClear:asAlert;
    }
    if(p>=json.size() || json[p++]!='"') return false;
    while(p<json.size() && whitespace(json[p])) ++p;
    if(p!=json.size()) return false;
    result=parsed;
    return true;
}
bool IsValidApiToken(const std::wstring& token) {
    if(token.empty() || token.size()>2048) return false;
    for(unsigned i=0;i<token.size();++i) if(token[i]<33 || token[i]>126) return false;
    return true;
}
unsigned long RetryDelaySeconds(unsigned status, const std::string& retryAfter) {
    if(status!=429 && status!=503) return 60;
    unsigned long seconds=0;
    if(retryAfter.empty()) return 60;
    for(unsigned i=0;i<retryAfter.size();++i) {
        if(retryAfter[i]<'0' || retryAfter[i]>'9') return 60;
        seconds=seconds*10+(retryAfter[i]-'0');
        if(seconds>=3600) return 3600;
    }
    return seconds<60?60:seconds;
}
AlertFeed::AlertFeed() : available(false),lastConfirmedTick(0) {}
void AlertFeed::Accept(const AlertSnapshot& value,unsigned long tick) {
    snapshot=value; available=true; lastConfirmedTick=tick;
}
void AlertFeed::Unavailable() { snapshot=AlertSnapshot(); available=false; }
bool AlertFeed::Expire(unsigned long tick) {
    // DWORD subtraction deliberately supports GetTickCount's 49-day wrap.
    if(!available || (unsigned long)(tick-lastConfirmedTick)<90000UL) return false;
    Unavailable();
    return true;
}
