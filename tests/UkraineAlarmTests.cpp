#include "../src/UkraineAlarmFeed.h"
#include "../src/AlertFeed.h"
#include <stdio.h>
#include <stdlib.h>

static int checks=0;
static void check(bool ok,const char* name) {
    ++checks;
    if(!ok) { fprintf(stderr,"FAIL: %s\n",name); exit(1); }
}
static const TerritoryStatus* territory(const AlertSnapshot& s,const wchar_t* id) {
    for(unsigned i=0;i<s.territories.size();++i) if(s.territories[i].id==id) return &s.territories[i];
    return 0;
}
int main() {
    // Names use the provider's shortened form. IDs are deliberately synthetic.
    const std::string directory=
        "{\"states\":["
        "{\"regionId\":\"KH\",\"regionName\":\"\\u0425\\u0430\\u0440\\u043a\\u0456\\u0432\\u0441\\u044c\\u043a\\u0430\",\"regionChildIds\":["
          "{\"regionId\":\"district\",\"regionName\":\"District\",\"regionChildIds\":["
             "{\"regionId\":\"village\",\"regionName\":\"Community\",\"regionChildIds\":[]}]}]},"
        "{\"regionId\":\"KV\",\"regionName\":\"\\u041a\\u0438\\u0457\\u0432\\u0441\\u044c\\u043a\\u0430\",\"regionChildIds\":[]},"
        "{\"regionId\":\"KC\",\"regionName\":\"\\u043c.\\u041a\\u0438\\u0457\\u0432\",\"regionChildIds\":[]},"
        "{\"regionId\":\"LV\",\"regionName\":\"\\u041b\\u044c\\u0432\\u0456\\u0432\\u0441\\u044c\\u043a\\u0430 \\u043e\\u0431\\u043b\\u0430\\u0441\\u0442\\u044c\",\"regionChildIds\":[]}]}";
    AlertSnapshot s;
    check(ParseUkraineAlarmCoverage("[]",directory,s),"empty active list with known coverage is valid");
    check(s.states[7]==asClear && s.states[11]==asClear,"absent covered oblasts are clear");
    check(s.states[4]==asClear && s.states[25]==asClear,"Kyiv city and oblast have distinct coverage");
    check(s.states[18]==asUnknown,"uncovered Crimea remains unknown");
    check(ParseUkraineAlarmCoverage("[{\"regionId\":\"KH\",\"activeAlerts\":[{\"regionId\":\"KH\",\"type\":\"AIR\"}]}]",directory,s),"map whole oblast by ID without name");
    check(s.states[7]==asAlert && s.levels[7]=='R' && s.states[11]==asClear,"only active oblast turns red");
    check(ParseUkraineAlarmCoverage("[{\"regionId\":\"village\",\"activeAlerts\":[{\"regionId\":\"village\",\"type\":\"AIR\"}]}]",directory,s),"map nested community to parent oblast");
    check(s.states[7]==asAlert && s.levels[7]=='P',"community alert produces partial status");
    check(ParseUkraineAlarmCoverage("[{\"regionId\":\"KH\",\"activeAlerts\":[{\"regionId\":\"KH\",\"type\":\"AIR\"},{\"regionId\":\"village\",\"type\":\"AIR\"}]}]",directory,s),"aggregate whole and child alerts");
    check(s.levels[7]=='R',"whole-region alert takes precedence over partial");
    check(ParseUkraineAlarmCoverage("[{\"regionId\":\"KC\",\"activeAlerts\":[{\"regionId\":\"KC\",\"type\":\"AIR\"}]}]",directory,s),"Kyiv city air alert");
    check(s.states[25]==asAlert && s.states[4]==asClear,"city alert does not color Kyiv oblast");
    check(ParseUkraineAlarmCoverage("[{\"regionId\":\"LV\",\"activeAlerts\":[{\"type\":\"ARTILLERY\"}]}]",directory,s),"non-air threat is accepted");
    check(s.states[11]==asClear,"non-air threat does not fabricate air alert");
    check(!ParseUkraineAlarmCoverage("[{\"regionId\":\"new-id\",\"activeAlerts\":[{\"regionId\":\"new-id\",\"type\":\"AIR\"}]}]",directory,s),"unmapped air alert rejects the snapshot");
    check(s.states[11]==asClear,"failed parse does not partly overwrite snapshot");
    check(!ParseUkraineAlarmCoverage("[]","{\"states\":[]}",s),"empty directory cannot make all Ukraine clear");
    check(!ParseUkraineAlarmCoverage("[]","{}",s),"missing directory schema rejected");
    check(!ParseUkraineAlarmCoverage("[{\"regionId\":\"KH\",\"activeAlerts\":null}]",directory,s),"null alerts rejected");
    check(!ParseUkraineAlarmCoverage("{}",directory,s),"API error object rejected");
    check(!ParseUkraineAlarmCoverage("[] trailing",directory,s),"trailing JSON rejected");

    std::string extended=directory.substr(0,directory.size()-2)+",{\"regionId\":\"village\",\"regionName\":\"\\u043c. \\u0425\\u0430\\u0440\\u043a\\u0456\\u0432 \\u0442\\u0430 \\u0425\\u0430\\u0440\\u043a\\u0456\\u0432\\u0441\\u044c\\u043a\\u0430 \\u0442\\u0435\\u0440\\u0438\\u0442\\u043e\\u0440\\u0456\\u0430\\u043b\\u044c\\u043d\\u0430 \\u0433\\u0440\\u043e\\u043c\\u0430\\u0434\\u0430\",\"regionType\":\"State\",\"regionChildIds\":[]},{\"regionId\":\"ZP\",\"regionName\":\"\\u0417\\u0430\\u043f\\u043e\\u0440\\u0456\\u0437\\u044c\\u043a\\u0430 \\u043e\\u0431\\u043b\\u0430\\u0441\\u0442\\u044c\",\"regionType\":\"State\",\"regionChildIds\":[]},{\"regionId\":\"ZCITY\",\"regionName\":\"\\u043c. \\u0417\\u0430\\u043f\\u043e\\u0440\\u0456\\u0436\\u0436\\u044f \\u0442\\u0430 \\u0417\\u0430\\u043f\\u043e\\u0440\\u0456\\u0437\\u044c\\u043a\\u0430 \\u0442\\u0435\\u0440\\u0438\\u0442\\u043e\\u0440\\u0456\\u0430\\u043b\\u044c\\u043d\\u0430 \\u0433\\u0440\\u043e\\u043c\\u0430\\u0434\\u0430\",\"regionType\":\"State\",\"regionChildIds\":[]},{\"regionId\":\"0\",\"regionName\":\"\\u0422\\u0435\\u0441\\u0442\\u043e\\u0432\\u0438\\u0439 \\u0440\\u0435\\u0433\\u0456\\u043e\\u043d\",\"regionType\":\"State\",\"regionChildIds\":[]}]}";
    check(ParseUkraineAlarmCoverage("[]",extended,s),"separate city roots merge with existing hierarchy");
    check(ParseUkraineAlarmCoverage("[{\"regionId\":\"village\",\"activeAlerts\":[{\"regionId\":\"village\",\"type\":\"AIR\"}]}]",extended,s),"Kharkiv city root air alert");
    check(s.levels[7]=='P',"Kharkiv city is partial coverage of Kharkiv oblast");
    check(ParseUkraineAlarmCoverage("[{\"regionId\":\"ZCITY\",\"activeAlerts\":[{\"regionId\":\"ZCITY\",\"type\":\"AIR\"}]}]",extended,s),"Zaporizhzhia city root air alert");
    check(s.levels[10]=='P',"Zaporizhzhia city is partial coverage of Zaporizhzhia oblast");
    check(territory(s,L"ZCITY")->parentId==L"ZP","separate city root links to its oblast for inheritance");
    check(ParseUkraineAlarmCoverage("[{\"regionId\":\"ZP\",\"activeAlerts\":[{\"type\":\"AIR\"}]}]",extended,s),"whole oblast includes separate city root");
    check(territory(s,L"ZCITY")->level=='R' && territory(s,L"ZCITY")->sourceId==L"ZP","whole oblast alarm reaches separately listed city");
    check(ParseUkraineAlarmCoverage("[{\"regionId\":\"0\",\"activeAlerts\":[{\"regionId\":\"0\",\"type\":\"AIR\"}]}]",extended,s),"provider test region does not invalidate real data");
    check(s.states[7]==asClear && s.states[10]==asClear,"provider test alert does not color real oblasts");
    std::string siblings=directory;
    std::string needle="\"regionId\":\"village\",\"regionName\":\"Community\",\"regionChildIds\":[]}";
    siblings.replace(siblings.find(needle),needle.size(),needle+",{\"regionId\":\"neighbor\",\"regionName\":\"Neighbor\",\"regionChildIds\":[]}");
    check(ParseUkraineAlarmCoverage("[{\"regionId\":\"village\",\"activeAlerts\":[{\"type\":\"AIR\"}]}]",siblings,s),"detailed community snapshot");
    check(territory(s,L"village") && territory(s,L"village")->level=='R',"active community retains its own status");
    check(territory(s,L"neighbor")->level=='N',"neighbor community remains clear");
    check(territory(s,L"district")->partial && territory(s,L"district")->level=='P',"district records partial coverage separately");
    check(territory(s,L"village")->parentId==L"district" && territory(s,L"village")->oblast==7,"stable ID and parent survive parsing");
    check(!territory(s,L"village")->severityKnown,"v3 absent severity is explicitly unspecified");
    check(ParseUkraineAlarmCoverage("[{\"regionId\":\"village\",\"activeAlerts\":[{\"type\":\"AIR\",\"activeAlertLevels\":[{\"alertLevel\":\"Yellow\"}]}]}]",siblings,s),"official nested severity schema");
    check(territory(s,L"village")->level=='Y' && territory(s,L"village")->severityKnown,"true yellow stays distinct from partial coverage");
    check(s.levels[7]=='P' && territory(s,L"neighbor")->level=='N',"yellow community does not color the whole oblast yellow");
    check(ParseUkraineAlarmCoverage("[{\"regionId\":\"KH\",\"activeAlerts\":[{\"type\":\"AIR\",\"activeAlertLevels\":[{\"alertLevel\":\"Yellow\"},{\"alertLevel\":\"Red\"}]}]}]",siblings,s),"multiple official severity records");
    check(territory(s,L"neighbor")->level=='R' && territory(s,L"neighbor")->severityKnown,"red takes precedence and is inherited");
    check(ParseUkraineAlarmCoverage("[{\"regionId\":\"village\",\"activeAlerts\":[{\"type\":\"AIR\",\"activeAlertLevels\":[{\"alertLevel\":\"FutureLevel\"}]}]}]",siblings,s),"unknown severity preserves active alarm");
    check(territory(s,L"village")->level=='R' && !territory(s,L"village")->severityKnown,"unknown severity never fabricates a clear state");
    check(ParseUkraineAlarmCoverage("[{\"regionId\":\"district\",\"activeAlerts\":[{\"type\":\"AIR\"}]}]",siblings,s),"district alarm snapshot");
    check(territory(s,L"neighbor")->level=='R' && territory(s,L"neighbor")->sourceId==L"district","district alarm inherits to all its communities");
    check(ParseUkraineAlarmCoverage("[{\"regionId\":\"KH\",\"activeAlerts\":[{\"type\":\"AIR\"}]}]",siblings,s),"oblast alarm snapshot");
    check(territory(s,L"neighbor")->level=='R' && territory(s,L"neighbor")->sourceId==L"KH","oblast alarm inherits through multiple levels");
    check(ParseUkraineAlarmCoverage("[]",extended,s) && territory(s,L"village")->parentId==L"district","duplicate city root preserves canonical parent");
    AlertFeed detailedFeed; detailedFeed.Accept(s,1000); detailedFeed.Expire(91000);
    check(detailedFeed.snapshot.territories.empty(),"expired territory data is removed");
    std::string safe=PublicUkraineAlarmDiagnostic("{\"token\":\"do-not-save\",\"Authorization\":\"do-not-save\",\"states\":[]}");
    check(safe.find("do-not-save")==std::string::npos && safe.find("states")!=std::string::npos,"diagnostic excludes credential fields");
    printf("PASS: %d UkraineAlarm coverage checks\n",checks);
    return 0;
}
