#include "../src/Preferences.h"
#include <stdio.h>
#include <stdlib.h>
static int checks=0;
static void check(bool ok,const char* name) { ++checks; if(!ok){fprintf(stderr,"FAIL: %s\n",name);exit(1);} }
static std::string replace(std::string s,const std::string& from,const std::string& to) { unsigned p=s.find(from); if(p==std::string::npos){fprintf(stderr,"fixture missing\n");exit(1);}s.replace(p,from.size(),to);return s; }
int main() {
 std::string out="untouched",defaults=DefaultPreferences();
 check(NormalizePreferences(defaults,out),"default preferences validate");
 std::string saved=replace(defaults,"\"favorites\":[]","\"favorites\":[\"Community:UA6312027\",\"State:UA63\"]");
 saved=replace(saved,"\"home\":\"\"","\"home\":\"Community:UA6312027\"");
 check(NormalizePreferences(saved,out),"favorites and home persist by geometry code");
 check(!NormalizePreferences(replace(saved,"\"State:UA63\"","\"Community:UA6312027\""),out),"duplicate favorites rejected");
 check(!NormalizePreferences(replace(saved,"\"home\":\"Community:UA6312027\"","\"home\":\"Community:UA0000001\""),out),"home must be a favorite");
 check(!NormalizePreferences(replace(defaults,"\"brightness\":100","\"brightness\":200"),out),"brightness bounded");
 check(!NormalizePreferences(replace(defaults,"\"borderWidth\":100","\"borderWidth\":false"),out),"numeric settings reject booleans");
 check(!NormalizePreferences(replace(defaults,"\"w\":1000","\"w\":0"),out),"zero viewport rejected");
 check(!NormalizePreferences(replace(defaults,"\"x\":0","\"x\":1001"),out),"out of bounds viewport rejected");
 check(!NormalizePreferences(replace(defaults,"\"h\":700","\"h\":699"),out),"viewport aspect ratio validated");
 check(!NormalizePreferences(replace(defaults,"\"muted\"","\"external-theme\""),out),"unknown theme rejected");
 check(!NormalizePreferences(replace(defaults,"\"showLabels\":true","\"showLabels\":1"),out),"layer flags are booleans");
 check(!NormalizePreferences(replace(defaults,"\"version\":1","\"version\":2"),out),"unsupported version rejected");
 check(!NormalizePreferences(replace(defaults,"\"favorites\":[]","\"favorites\":[\"../token\"]"),out),"invalid geometry identifiers rejected");
 check(!NormalizePreferences(replace(defaults,"\"version\":1","\"version\":1,\"token\":\"do-not-persist\""),out),"unknown credential-like fields cannot persist");
 check(!NormalizePreferences(replace(defaults,"\"version\":1","\"version\":1,\"version\":1"),out),"duplicate fields rejected");
 check(!NormalizePreferences("{}",out)&&!NormalizePreferences(defaults+" trailing",out),"corrupt preference payload rejected");
 check(IsTerritoryKey("State:UA63")&&!IsTerritoryKey("State:UA63\n")&&!IsTerritoryKey("Community:UA"),"territory key grammar is exact");
 printf("PASS: %d preferences checks\n",checks);return 0;
}
