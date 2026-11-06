#include <vcl.h>
#include "../src/AppStorage.h"
#include "../src/HistoryReport.h"
#include "../src/MapData.h"
#include <System.JSON.hpp>
#include <stdio.h>
#include <memory>
using namespace System::Json;
int main() {
 wchar_t temporary[MAX_PATH],suffix[100];GetTempPathW(MAX_PATH,temporary);
 wsprintfW(suffix,L"AirAlertMap catalog integration %lu-%lu",GetCurrentProcessId(),GetTickCount());
 std::wstring folder=std::wstring(temporary)+suffix;CreateDirectoryW(folder.c_str(),NULL);
 int result=1;
 try {
  AppStorage storage(folder);AlertSnapshot snapshot;
  TerritoryStatus t;t.id=L"1293";t.parentId=L"22";t.oblast=7;t.type=L"Community";t.name=L"\u0425\u0430\u0440\u043a\u0456\u0432";t.level='N';snapshot.territories.push_back(t);
  if(!storage.AppendObservation(snapshot,true,apUkraineAlarm,1800000000i64))throw 1;
  snapshot.territories[0].level='Y';if(!storage.AppendObservation(snapshot,true,apUkraineAlarm,1800000060i64))throw 1;
  snapshot.territories[0].level='R';if(!storage.AppendObservation(snapshot,true,apUkraineAlarm,1800000120i64))throw 1;
  wchar_t script[MAX_PATH];GetFullPathNameW(L"..\\analytics\\analyze.py",MAX_PATH,script,NULL);
  TReportWorker* worker=new TReportWorker(script,folder,Regions[7].iso,0,"1293");worker->Start();worker->WaitFor();unsigned error=worker->Error;delete worker;if(error)throw 1;
  FILE* f=_wfopen((folder+L"\\reports\\history.json").c_str(),L"rb");if(!f)throw 1;
  char buffer[100000];unsigned count=fread(buffer,1,sizeof(buffer),f);fclose(f);
  std::auto_ptr<TJSONValue> root(TJSONObject::ParseJSONValue(System::UTF8String(buffer,count),false,true));
  TJSONObject* object=dynamic_cast<TJSONObject*>(root.get());TJSONObject* child=object?dynamic_cast<TJSONObject*>(object->GetValue(L"territory")):0;
  if(!child || child->GetValue(L"id")->Value()!=L"1293" || child->GetValue(L"clear")->Value()!=L"60" || child->GetValue(L"threat")->Value()!=L"60" || child->GetValue(L"unknown")->Value()!=L"0" || child->GetValue(L"starts")->Value()!=L"1")throw 1;
  puts("PASS: native catalog hash and compact history match Python selected-territory analytics");result=0;
 }catch(...){fprintf(stderr,"FAIL: native catalog / Python interoperability\n");}
 // All paths originate in this test's own unique, verified temporary directory.
 WIN32_FIND_DATAW entry;HANDLE search=FindFirstFileW((folder+L"\\*").c_str(),&entry);
 if(search!=INVALID_HANDLE_VALUE){do{if(!(entry.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))DeleteFileW((folder+L"\\"+entry.cFileName).c_str());}while(FindNextFileW(search,&entry));FindClose(search);}
 const wchar_t* reports[]={L"history.json",L"history.html",L"history.csv",L"territory.csv",L"territory-daily.csv",L"territory-events.csv"};
 for(unsigned i=0;i<6;++i)DeleteFileW((folder+L"\\reports\\"+reports[i]).c_str());RemoveDirectoryW((folder+L"\\reports").c_str());RemoveDirectoryW(folder.c_str());
 return result;
}
