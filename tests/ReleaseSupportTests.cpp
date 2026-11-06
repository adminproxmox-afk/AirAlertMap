#include "../src/ReleaseSupport.h"
#include <stdio.h>
#include <stdlib.h>
static int checks=0;
static void check(bool ok,const char* name){++checks;if(!ok){fprintf(stderr,"FAIL: %s\n",name);exit(1);}}
static void put(const std::wstring& name,const char* data){FILE* f=_wfopen(name.c_str(),L"wb");if(!f)exit(1);fwrite(data,1,strlen(data),f);fclose(f);}
int main(){
 wchar_t root[MAX_PATH],suffix[80];GetTempPathW(MAX_PATH,root);wsprintfW(suffix,L"AirAlertMap support %lu-%lu",GetCurrentProcessId(),GetTickCount());std::wstring folder=std::wstring(root)+suffix;CreateDirectoryW(folder.c_str(),NULL);
 put(folder+L"\\history-2026-11.jsonl","12345");put(folder+L"\\catalog-0123456789abcdef.json","123");put(folder+L"\\preferences.json","private-setting");put(folder+L"\\token.json","secret-sentinel");put(folder+L"\\history-2026-99.jsonl","invalid-month");
 HistoryStorageStats stats=InspectHistoryStorage(folder);
 check(stats.files==1&&stats.catalogs==1&&stats.bytes==8&&stats.catalogBytes==3,"storage counts only canonical observations and catalogs");
 DiagnosticState d;d.storage=stats;d.httpStatus=401;d.apiError=aeToken;d.confirmedAt=1800000000;d.keyConfigured=true;d.pythonAvailable=true;
 System::UnicodeString diagnostic=BuildDiagnostics(d);
 check(diagnostic.Pos(L"401")>0&&diagnostic.Pos(L"ukrainealarm")>0,"diagnostic contains controlled connection evidence");
 check(diagnostic.Pos(L"secret-sentinel")==0&&diagnostic.Pos(L"private-setting")==0&&diagnostic.Pos(folder.c_str())==0,"diagnostic excludes history, settings, secrets and private paths");
 check(diagnostic.Pos(L"Authorization")==0&&diagnostic.Pos(L"favorites")==0&&diagnostic.Pos(L"selected")==0,"diagnostic schema excludes headers and location");
 const wchar_t* files[]={L"history-2026-11.jsonl",L"catalog-0123456789abcdef.json",L"preferences.json",L"token.json",L"history-2026-99.jsonl"};for(unsigned i=0;i<5;++i)DeleteFileW((folder+L"\\"+files[i]).c_str());RemoveDirectoryW(folder.c_str());
 printf("PASS: %d release support checks\n",checks);return 0;
}
