#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "../src/AppStorage.h"
static int checks=0;
static void check(bool value,const char* name) {
    ++checks; if(!value) { fprintf(stderr,"FAIL: %s\n",name); throw 1; }
}
static void Put(const std::wstring& folder,const wchar_t* name,const char* body) {
    FILE* f=_wfopen((folder+L"\\"+name).c_str(),L"wb");
    if(!f) throw 1; fwrite(body,1,strlen(body),f); fclose(f);
}
int main() {
    wchar_t tmp[MAX_PATH],name[80]; GetTempPathW(MAX_PATH,tmp);
    wsprintfW(name,L"AirAlertMap.StorageTests-%lu-%lu",GetCurrentProcessId(),GetTickCount());
    std::wstring folder=std::wstring(tmp)+name; CreateDirectoryW(folder.c_str(),NULL);
    int result=0;
    try {
        AppStorage disk(folder); std::string directory="unchanged";
        const __int64 now=1800000000;
        const std::string valid="{\"states\":[{\"regionId\":\"KC\",\"regionName\":\"\\u041a\\u0438\\u0457\\u0432\",\"regionChildIds\":[]}],\"private\":\"do-not-save\"}";
        check(!disk.LoadDirectory(directory,now) && directory=="unchanged","missing cache preserves caller value");
        check(disk.SaveDirectory(valid,now),"save validated directory");
        AppStorage restarted(folder);
        check(restarted.LoadDirectory(directory,now+10),"new storage instance restores directory");
        check(directory.find("do-not-save")==std::string::npos && PrepareRegionDirectory(directory).error==aePreparing,"only valid public directory fields persisted");
        check(!disk.LoadDirectory(directory,now+86400),"directory expires after 24 hours");
        check(!disk.LoadDirectory(directory,now-1),"future cache timestamp rejected");
        check(!disk.SaveDirectory("{}",now),"invalid directory cannot replace cache");
        check(disk.LoadDirectory(directory,now+20),"failed save preserves previous cache");
        disk.InvalidateDirectory(); check(!disk.LoadDirectory(directory,now+20),"format failure invalidates directory");
        Put(folder,L"regions.cache","AirAlertMap directory 1\nnot-a-time\n{}");
        check(!disk.LoadDirectory(directory,now),"corrupt cache timestamp rejected");
        check(disk.LoadSelectedRegion()==7,"default region when settings absent");
        check(disk.SaveSelectedRegion(25),"save Kyiv city independently of Kyiv oblast");
        check(restarted.LoadSelectedRegion()==25,"selected region restored after restart");
        check(!disk.SaveSelectedRegion(27) && restarted.LoadSelectedRegion()==25,"invalid selection cannot replace previous setting");
        Put(folder,L"settings.txt","AirAlertMap settings 1\nUNKNOWN");
        check(disk.LoadSelectedRegion(10)==10,"unknown ISO falls back safely");
        check(disk.SaveRetry(apUkraineAlarm,180,now),"save API retry deadline");
        check(restarted.LoadRetry(apUkraineAlarm,now+30)==150,"restart respects remaining Retry-After");
        check(disk.LoadRetry(apAlertsInUa,now)==0,"provider deadlines are separate");
        check(disk.LoadRetry(apUkraineAlarm,now+180)==0,"expired deadline allows request");
        check(disk.LoadRetry(apUkraineAlarm,now-4000)==0,"implausible future deadline ignored");
        AppStorage unwritable(folder+L"\\missing\\nested");
        check(!unwritable.SaveSelectedRegion(1),"disk failure is reported without crashing");
        AlertSnapshot snapshot; snapshot.states[0]=asAlert; snapshot.levels[0]='P';
        check(PublicLevels(snapshot,true)==std::string("P")+std::string(26,'?'),"history preserves partial alert and missing coverage");
        check(PublicLevels(snapshot,false)==std::string(27,'?'),"unavailable sample never preserves active or clear statuses");
        check(disk.AppendObservation(snapshot,true,apUkraineAlarm,now),"append confirmed observation");
        check(disk.AppendObservation(snapshot,false,apUkraineAlarm,now+60),"append outage boundary");
        check(!disk.AppendObservation(snapshot,true,(ApiProvider)99,now),"invalid provider cannot enter history");
        printf("PASS: %d storage checks\n",checks);
    } catch(...) { result=1; }
    const wchar_t* files[]={L"regions.cache",L"settings.txt",L"ukrainealarm.retry",L"alerts.retry"};
    for(unsigned i=0;i<4;++i) DeleteFileW((folder+L"\\"+files[i]).c_str());
    DeleteFileW((folder+L"\\history-2027-01.jsonl").c_str());
    RemoveDirectoryW(folder.c_str());
    return result;
}
