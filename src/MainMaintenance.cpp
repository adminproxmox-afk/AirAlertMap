#include <vcl.h>
#pragma hdrstop
#include "MainForm.h"
#include <Vcl.Dialogs.hpp>
#include <Vcl.Clipbrd.hpp>
#include <System.JSON.hpp>
#include <memory>
#include <stdio.h>
using namespace System::Json;
void TMainForm::RefreshStorage(bool preserveMessage) {
    if(!preserveMessage && !storageWorker){storageError=false;storageMessage=L"";}
    historyStorage=InspectHistoryStorage(storage.Folder());++storageRevision;
    if(historyStorage.error){storageError=true;storageMessage=L"\u041d\u0435 \u0432\u0434\u0430\u043b\u043e\u0441\u044f \u043f\u0435\u0440\u0435\u0432\u0456\u0440\u0438\u0442\u0438 \u0440\u043e\u0437\u043c\u0456\u0440 \u0456\u0441\u0442\u043e\u0440\u0456\u0457.";}
}
void TMainForm::CopyDiagnostics() {
    ++diagnosticsRevision;diagnosticsError=false;
    try {Vcl::Clipbrd::Clipboard()->AsText=diagnostics;diagnosticsMessage=L"\u0414\u0456\u0430\u0433\u043d\u043e\u0441\u0442\u0438\u043a\u0443 \u0441\u043a\u043e\u043f\u0456\u0439\u043e\u0432\u0430\u043d\u043e.";}
    catch(...){diagnosticsError=true;diagnosticsMessage=L"\u0411\u0443\u0444\u0435\u0440 \u043e\u0431\u043c\u0456\u043d\u0443 \u0437\u0430\u0439\u043d\u044f\u0442\u0438\u0439. \u0421\u043f\u0440\u043e\u0431\u0443\u0439\u0442\u0435 \u0449\u0435 \u0440\u0430\u0437.";}
}
void TMainForm::StartStorage(const std::string& action,const std::wstring& destination) {
    ++storageRevision;storageError=true;if(action=="prune")lastCleanupAttempt=UtcSeconds();
    if(storageWorker||reportWorker){storageMessage=L"\u0414\u043e\u0447\u0435\u043a\u0430\u0439\u0442\u0435\u0441\u044f \u0437\u0430\u0432\u0435\u0440\u0448\u0435\u043d\u043d\u044f \u043f\u043e\u0442\u043e\u0447\u043d\u043e\u0457 \u043e\u0431\u0440\u043e\u0431\u043a\u0438 \u0456\u0441\u0442\u043e\u0440\u0456\u0457.";return;}
    if(!pythonAvailable){storageMessage=L"\u0414\u043b\u044f \u0430\u0440\u0445\u0456\u0432\u0443 \u0442\u0430 \u043e\u0447\u0438\u0449\u0435\u043d\u043d\u044f \u043f\u043e\u0442\u0440\u0456\u0431\u0435\u043d Python 3.10+. \u0406\u0441\u0442\u043e\u0440\u0456\u044f \u0437\u0430\u043b\u0438\u0448\u0430\u0454\u0442\u044c\u0441\u044f \u0437\u0431\u0435\u0440\u0435\u0436\u0435\u043d\u043e\u044e.";return;}
    UnicodeString script=ExtractFilePath(Application->ExeName)+L"analytics\\manage_history.py";
    if(!FileExists(script)){storageMessage=L"\u041c\u043e\u0434\u0443\u043b\u044c \u043e\u0431\u0440\u043e\u0431\u043a\u0438 \u0456\u0441\u0442\u043e\u0440\u0456\u0457 \u0432\u0456\u0434\u0441\u0443\u0442\u043d\u0456\u0439. \u041f\u0435\u0440\u0435\u0432\u0456\u0440\u0442\u0435 \u0444\u0430\u0439\u043b\u0438 \u0437\u0431\u0456\u0440\u043a\u0438.";return;}
    try {
        RecordObservation(false);storageAction=action;
        storageWorker=new TStorageWorker(script.c_str(),storage.Folder(),action,retentionDays,UtcSeconds(),destination);storageWorker->Start();
        storageError=false;storageMessage=action=="archive"?UnicodeString(L"\u0421\u0442\u0432\u043e\u0440\u044e\u0454\u043c\u043e \u0430\u0440\u0445\u0456\u0432 \u0456\u0441\u0442\u043e\u0440\u0456\u0457\u2026"):UnicodeString(L"\u041e\u0447\u0438\u0449\u0443\u0454\u043c\u043e \u0441\u0442\u0430\u0440\u0456 \u0441\u043f\u043e\u0441\u0442\u0435\u0440\u0435\u0436\u0435\u043d\u043d\u044f\u2026");
    }catch(...){delete storageWorker;storageWorker=0;storageError=true;storageMessage=L"\u041d\u0435 \u0432\u0434\u0430\u043b\u043e\u0441\u044f \u0437\u0430\u043f\u0443\u0441\u0442\u0438\u0442\u0438 \u043e\u0431\u0440\u043e\u0431\u043a\u0443 \u0456\u0441\u0442\u043e\u0440\u0456\u0457.";RecordObservation(feed.available);}
}
void TMainForm::ChangeRetention(unsigned days) {
    ++storageRevision;storageError=true;
    if(storageWorker||reportWorker){storageMessage=L"\u0414\u043e\u0447\u0435\u043a\u0430\u0439\u0442\u0435\u0441\u044f \u0437\u0430\u0432\u0435\u0440\u0448\u0435\u043d\u043d\u044f \u043f\u043e\u0442\u043e\u0447\u043d\u043e\u0457 \u043e\u0431\u0440\u043e\u0431\u043a\u0438.";return;}
    if(days&&!pythonAvailable){storageMessage=L"\u0414\u043b\u044f \u043e\u0447\u0438\u0449\u0435\u043d\u043d\u044f \u043f\u043e\u0442\u0440\u0456\u0431\u0435\u043d Python 3.10+. \u041f\u0440\u0430\u0432\u0438\u043b\u043e \u043d\u0435 \u0437\u043c\u0456\u043d\u0435\u043d\u043e.";return;}
    if(!storage.SaveHistoryRetention(days)){storageMessage=L"\u041d\u0435 \u0432\u0434\u0430\u043b\u043e\u0441\u044f \u0437\u0431\u0435\u0440\u0435\u0433\u0442\u0438 \u043f\u0440\u0430\u0432\u0438\u043b\u043e \u0437\u0431\u0435\u0440\u0456\u0433\u0430\u043d\u043d\u044f.";return;}
    retentionDays=days;storageError=false;
    if(days)StartStorage("prune");else storageMessage=L"\u0406\u0441\u0442\u043e\u0440\u0456\u044f \u0437\u0431\u0435\u0440\u0456\u0433\u0430\u0454\u0442\u044c\u0441\u044f \u0431\u0435\u0437 \u043e\u0431\u043c\u0435\u0436\u0435\u043d\u043d\u044f \u0441\u0442\u0440\u043e\u043a\u0443.";
}
void TMainForm::ArchiveHistory() {
    ++storageRevision;storageError=false;
    if(storageWorker||reportWorker||!pythonAvailable){storageError=true;storageMessage=L"\u0410\u0440\u0445\u0456\u0432 \u043d\u0435\u0434\u043e\u0441\u0442\u0443\u043f\u043d\u0438\u0439: \u043f\u0435\u0440\u0435\u0432\u0456\u0440\u0442\u0435 Python \u0456 \u0434\u043e\u0447\u0435\u043a\u0430\u0439\u0442\u0435\u0441\u044f \u0437\u0430\u0432\u0435\u0440\u0448\u0435\u043d\u043d\u044f \u043e\u0431\u0440\u043e\u0431\u043a\u0438.";return;}
    std::auto_ptr<TSaveDialog> dialog(new TSaveDialog(this));dialog->Title=L"\u0410\u0440\u0445\u0456\u0432 \u0441\u043f\u043e\u0441\u0442\u0435\u0440\u0435\u0436\u0435\u043d\u044c";dialog->DefaultExt=L"zip";dialog->Filter=L"ZIP|*.zip";dialog->FileName=L"AirAlertMap-history-"+FormatDateTime(L"yyyymmdd",Now())+L".zip";dialog->Options=dialog->Options<<ofOverwritePrompt<<ofPathMustExist;
    if(dialog->Execute())StartStorage("archive",dialog->FileName.c_str());else storageMessage=L"\u0421\u0442\u0432\u043e\u0440\u0435\u043d\u043d\u044f \u0430\u0440\u0445\u0456\u0432\u0443 \u0441\u043a\u0430\u0441\u043e\u0432\u0430\u043d\u043e.";
}
void TMainForm::PollStorage() {
    if(!storageWorker||WaitForSingleObject((HANDLE)storageWorker->Handle,0)!=WAIT_OBJECT_0)return;
    unsigned error=storageWorker->Error;std::wstring output=storageWorker->Output;delete storageWorker;storageWorker=0;
    bool success=false;UnicodeString reason;
    FILE* f=_wfopen(output.c_str(),L"rb");if(f){char text[65537];unsigned n=fread(text,1,sizeof(text),f);bool valid=!ferror(f)&&n<sizeof(text);fclose(f);if(valid)try{std::auto_ptr<TJSONValue> root(TJSONObject::ParseJSONValue(UTF8String(text,n),false,true));TJSONObject* o=dynamic_cast<TJSONObject*>(root.get());TJSONBool* ok=o?dynamic_cast<TJSONBool*>(o->GetValue(L"ok")):0;TJSONString* action=o?dynamic_cast<TJSONString*>(o->GetValue(L"action")):0;TJSONNumber* version=o?dynamic_cast<TJSONNumber*>(o->GetValue(L"version")):0;success=error==0&&ok&&ok->AsBoolean&&action&&action->Value()==UnicodeString(storageAction.c_str())&&version&&version->Value()==L"1";TJSONString* e=o?dynamic_cast<TJSONString*>(o->GetValue(L"error")):0;if(e)reason=e->Value();}catch(...){}}
    DeleteFileW(output.c_str());++storageRevision;storageError=!success;
    if(success)storageMessage=storageAction=="archive"?UnicodeString(L"\u0410\u0440\u0445\u0456\u0432 \u0456\u0441\u0442\u043e\u0440\u0456\u0457 \u0437\u0431\u0435\u0440\u0435\u0436\u0435\u043d\u043e."):UnicodeString(L"\u041f\u0440\u0430\u0432\u0438\u043b\u043e \u0437\u0430\u0441\u0442\u043e\u0441\u043e\u0432\u0430\u043d\u043e. \u0421\u0442\u0430\u0440\u0456 \u0441\u043f\u043e\u0441\u0442\u0435\u0440\u0435\u0436\u0435\u043d\u043d\u044f \u043e\u0447\u0438\u0449\u0435\u043d\u043e.");
    else if(reason==L"invalid_history")storageMessage=L"\u0412 \u0456\u0441\u0442\u043e\u0440\u0456\u0457 \u0454 \u043f\u043e\u0448\u043a\u043e\u0434\u0436\u0435\u043d\u0456 \u0437\u0430\u043f\u0438\u0441\u0438. \u041e\u0447\u0438\u0449\u0435\u043d\u043d\u044f \u0441\u043a\u0430\u0441\u043e\u0432\u0430\u043d\u043e.";
    else if(reason==L"unsafe_path")storageMessage=L"\u041e\u0431\u0435\u0440\u0456\u0442\u044c \u0434\u043b\u044f \u0430\u0440\u0445\u0456\u0432\u0443 \u043f\u0430\u043f\u043a\u0443 \u043f\u043e\u0437\u0430 \u0434\u0430\u043d\u0438\u043c\u0438 \u0442\u0430 \u0444\u0430\u0439\u043b\u0430\u043c\u0438 \u043f\u0440\u043e\u0433\u0440\u0430\u043c\u0438.";
    else storageMessage=L"\u041d\u0435 \u0432\u0434\u0430\u043b\u043e\u0441\u044f \u0437\u0430\u0432\u0435\u0440\u0448\u0438\u0442\u0438 \u043e\u0431\u0440\u043e\u0431\u043a\u0443 \u0456\u0441\u0442\u043e\u0440\u0456\u0457. \u041f\u0435\u0440\u0435\u0432\u0456\u0440\u0442\u0435 \u0432\u0456\u043b\u044c\u043d\u0435 \u043c\u0456\u0441\u0446\u0435 \u0442\u0430 \u0434\u043e\u0441\u0442\u0443\u043f \u0434\u043e \u0444\u0430\u0439\u043b\u0456\u0432.";
    historyJson=L"";historyGeneratedAt=0;RecordObservation(feed.available);RefreshStorage(true);
    if(historyPending){historyPending=false;LoadHistory(pendingDays,pendingTerritory);}
}
