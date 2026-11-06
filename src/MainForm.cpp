#include <vcl.h>
#pragma hdrstop
#include "MainForm.h"
#include "MapData.h"
#include "ApiCredentials.h"
#include "HistoryReport.h"
#include <Vcl.Dialogs.hpp>
#include <System.JSON.hpp>
#include <memory>
#include <stdio.h>
#pragma package(smart_init)
#pragma resource "MainForm.dfm"
using namespace System::Json;
TMainForm* MainForm;
class TAlertWorker : public TThread {
    std::wstring token;
    std::string directory;
    ApiProvider provider;
protected:
    void __fastcall Execute() {
        try { result=FetchAlerts(token,provider,directory); }
        catch(...) { result=ApiResult(); result.networkError=ERROR_GEN_FAILURE; }
        token.clear();
    }
public:
    ApiResult result;
    ApiProvider Source() const { return provider; }
    unsigned generation;
    __fastcall TAlertWorker(const std::wstring& value,unsigned version,ApiProvider source,const std::string& schema)
      : TThread(true),token(value),directory(schema),provider(source),generation(version) { FreeOnTerminate=false; }
};

__fastcall TMainForm::TMainForm(TComponent* owner)
 : TWebMapForm(owner,ExtractFilePath(Application->ExeName)+L"web\\index.html",UnicodeString(AppStorage().Folder().c_str())+L"\\WebView2"),
 feedTimer(0),worker(0),reportWorker(0),provider(apUkraineAlarm),rememberKey(true),keySaved(false),demoMode(false),authPaused(false),historyOk(true),settingsError(false),detailsDirty(true),
 generation(0),settingsRevision(0),detailRevision(0),lastAttemptTick(GetTickCount()-60000UL),retryDelay(60000UL),selected(7),historyDays(7),confirmedAt(0),historyGeneratedAt(0) {
    Caption=L"AirAlertMap \u2014 \u041c\u0430\u043f\u0430 \u0442\u0440\u0438\u0432\u043e\u0433 \u0423\u043a\u0440\u0430\u0457\u043d\u0438";
    selected=storage.LoadSelectedRegion(selected);
    keySaved=LoadApiCredential(ApiCredentialTarget,apiToken,provider);
    if(!keySaved) {
        apiToken=System::Sysutils::GetEnvironmentVariable(L"UKRAINEALARM_API_KEY").c_str();
        if(apiToken.empty()) {
            apiToken=System::Sysutils::GetEnvironmentVariable(L"ALERTS_IN_UA_TOKEN").c_str();
            if(!apiToken.empty()) provider=apAlertsInUa;
        }
    }
    if(!IsValidApiToken(apiToken)) apiToken.clear();
    rememberKey=apiToken.empty() || keySaved;
    if(provider==apUkraineAlarm) storage.LoadDirectory(regionDirectory,UtcSeconds());
    unsigned remaining=storage.LoadRetry(provider,UtcSeconds());
    if(remaining) { lastAttemptTick=GetTickCount(); retryDelay=remaining*1000UL; }
    connectionText=apiToken.empty()?UnicodeString(L"\u0414\u043e\u0434\u0430\u0439\u0442\u0435 API-\u043a\u043b\u044e\u0447 \u0443 \u043d\u0430\u043b\u0430\u0448\u0442\u0443\u0432\u0430\u043d\u043d\u044f\u0445 \u0434\u043b\u044f \u0437\u0430\u0432\u0430\u043d\u0442\u0430\u0436\u0435\u043d\u043d\u044f \u0430\u043a\u0442\u0443\u0430\u043b\u044c\u043d\u0438\u0445 \u0434\u0430\u043d\u0438\u0445."):UnicodeString(L"\u041e\u0447\u0456\u043a\u0443\u0432\u0430\u043d\u043d\u044f \u043f\u0435\u0440\u0448\u043e\u0433\u043e \u0437\u0430\u043f\u0438\u0442\u0443 \u0434\u043e API\u2026");
    RecordObservation(false);
    OnCommand=Command;
    feedTimer=new TTimer(this); feedTimer->Interval=250; feedTimer->OnTimer=FeedTick; feedTimer->Enabled=true;
    PublishState();
}
__fastcall TMainForm::~TMainForm() {
    if(feedTimer) feedTimer->Enabled=false;
    OnCommand=0;
    if(worker) { worker->WaitFor(); delete worker; worker=0; }
    if(reportWorker) { reportWorker->WaitFor(); delete reportWorker; reportWorker=0; }
    RecordObservation(false);
}
void TMainForm::RecordObservation(bool available) {
    historyOk=storage.AppendObservation(feed.snapshot,available && !demoMode,provider,UtcSeconds());
}
void TMainForm::ApplyFeed() {
    detailsDirty=true; ++detailRevision;
    for(int i=0;i<RegionCount;++i) model.states[i]=feed.snapshot.states[i];
    RecordObservation(feed.available); PublishState();
}
void TMainForm::SetMode(bool live) {
    if(demoMode==!live) return;
    detailsDirty=true; ++detailRevision;
    ++generation; demoMode=!live; feed.Unavailable(); RecordObservation(false);
    if(demoMode) { model.ResetDemo(); connectionText=L"\u0414\u0435\u043c\u043e\u0440\u0435\u0436\u0438\u043c: \u0432\u0438\u0433\u0430\u0434\u0430\u043d\u0456 \u0441\u0442\u0430\u043d\u0438. \u0417\u0430\u043f\u0438\u0442\u0438 API \u043f\u0440\u0438\u0437\u0443\u043f\u0438\u043d\u0435\u043d\u043e."; }
    else { for(int i=0;i<RegionCount;++i) model.states[i]=asUnknown; connectionText=L"\u041e\u0447\u0456\u043a\u0443\u0432\u0430\u043d\u043d\u044f \u043d\u0430\u0441\u0442\u0443\u043f\u043d\u043e\u0433\u043e \u0434\u043e\u0437\u0432\u043e\u043b\u0435\u043d\u043e\u0433\u043e \u0437\u0430\u043f\u0438\u0442\u0443 \u0434\u043e API\u2026"; }
}
void TMainForm::SaveSettings(const UiCommand& command) {
    ++settingsRevision; settingsError=true;
    std::wstring next=command.token;
    if(next.empty()) {
        if(command.provider!=provider || apiToken.empty()) { settingsMessage=L"\u0412\u0432\u0435\u0434\u0456\u0442\u044c \u043a\u043b\u044e\u0447 \u0434\u043b\u044f \u043e\u0431\u0440\u0430\u043d\u043e\u0433\u043e \u043f\u043e\u0441\u0442\u0430\u0447\u0430\u043b\u044c\u043d\u0438\u043a\u0430."; return; }
        next=apiToken;
    }
    bool stored=command.remember?SaveApiCredential(ApiCredentialTarget,next,command.provider):DeleteApiCredential(ApiCredentialTarget);
    if(!stored) { settingsMessage=L"Windows \u043d\u0435 \u0437\u043c\u0456\u0433 \u0437\u0431\u0435\u0440\u0435\u0433\u0442\u0438 \u043d\u0430\u043b\u0430\u0448\u0442\u0443\u0432\u0430\u043d\u043d\u044f \u043a\u043b\u044e\u0447\u0430. \u041f\u043e\u043f\u0435\u0440\u0435\u0434\u043d\u0456 \u043d\u0430\u043b\u0430\u0448\u0442\u0443\u0432\u0430\u043d\u043d\u044f \u0437\u0430\u043b\u0438\u0448\u0438\u043b\u0438\u0441\u044f \u0447\u0438\u043d\u043d\u0438\u043c\u0438."; return; }
    apiToken=next; provider=command.provider; rememberKey=command.remember; keySaved=command.remember;
    ++generation; authPaused=false; feed.Unavailable(); confirmedAt=0; regionDirectory.clear();
    if(provider==apUkraineAlarm) storage.LoadDirectory(regionDirectory,UtcSeconds());
    unsigned remaining=storage.LoadRetry(provider,UtcSeconds());
    lastAttemptTick=GetTickCount()-(remaining?0:60000UL); retryDelay=remaining?remaining*1000UL:60000UL;
    settingsError=false; settingsMessage=keySaved?UnicodeString(L"\u041a\u043b\u044e\u0447 \u0456 \u043f\u043e\u0441\u0442\u0430\u0447\u0430\u043b\u044c\u043d\u0438\u043a \u0437\u0431\u0435\u0440\u0435\u0436\u0435\u043d\u0456 \u0443 Windows."):UnicodeString(L"\u041a\u043b\u044e\u0447 \u0437\u0430\u0441\u0442\u043e\u0441\u043e\u0432\u0430\u043d\u043e \u043b\u0438\u0448\u0435 \u0434\u043b\u044f \u043f\u043e\u0442\u043e\u0447\u043d\u043e\u0433\u043e \u0441\u0435\u0430\u043d\u0441\u0443.");
    connectionText=L"\u041d\u0430\u043b\u0430\u0448\u0442\u0443\u0432\u0430\u043d\u043d\u044f \u0437\u0430\u0441\u0442\u043e\u0441\u043e\u0432\u0430\u043d\u043e. \u041e\u0447\u0456\u043a\u0443\u0432\u0430\u043d\u043d\u044f \u0434\u043e\u0437\u0432\u043e\u043b\u0435\u043d\u043e\u0433\u043e \u0437\u0430\u043f\u0438\u0442\u0443 API\u2026";
    if(!demoMode) ApplyFeed();
}
void __fastcall TMainForm::Command(TObject*,const UnicodeString& message) {
    UiCommand command;
    if(!ParseUiCommand(message,command)) {
        ++settingsRevision; settingsError=true; settingsMessage=L"\u041d\u0435 \u0432\u0434\u0430\u043b\u043e\u0441\u044f \u0437\u0430\u0441\u0442\u043e\u0441\u0443\u0432\u0430\u0442\u0438 \u0434\u0456\u044e: \u043f\u0435\u0440\u0435\u0432\u0456\u0440\u0442\u0435 \u0444\u043e\u0440\u043c\u0430\u0442 \u0432\u0432\u0435\u0434\u0435\u043d\u0438\u0445 \u0434\u0430\u043d\u0438\u0445."; PublishState(); return;
    }
    try {
        switch(command.type) {
            case ucReady: detailsDirty=true; break;
            case ucSelect: selected=command.region; storage.SaveSelectedRegion(selected); break;
            case ucRefresh: StartRequest(); break;
            case ucMode: SetMode(command.live); break;
            case ucSaveSettings: SaveSettings(command); break;
            case ucForgetKey:
                ++settingsRevision;
                if(!DeleteApiCredential(ApiCredentialTarget)) { settingsError=true; settingsMessage=L"\u041d\u0435 \u0432\u0434\u0430\u043b\u043e\u0441\u044f \u0432\u0438\u0434\u0430\u043b\u0438\u0442\u0438 \u0437\u0431\u0435\u0440\u0435\u0436\u0435\u043d\u0438\u0439 \u043a\u043b\u044e\u0447 \u0443 Windows."; break; }
                ++generation; apiToken.clear(); keySaved=false; rememberKey=false; authPaused=false; feed.Unavailable(); confirmedAt=0;
                settingsError=false; settingsMessage=L"\u041a\u043b\u044e\u0447 \u0432\u0438\u0434\u0430\u043b\u0435\u043d\u043e. \u0414\u043b\u044f \u043f\u0456\u0434\u043a\u043b\u044e\u0447\u0435\u043d\u043d\u044f \u0432\u0432\u0435\u0434\u0456\u0442\u044c \u043d\u043e\u0432\u0438\u0439 \u043a\u043b\u044e\u0447.";
                connectionText=L"\u0414\u043e\u0434\u0430\u0439\u0442\u0435 API-\u043a\u043b\u044e\u0447 \u0443 \u043d\u0430\u043b\u0430\u0448\u0442\u0443\u0432\u0430\u043d\u043d\u044f\u0445."; if(!demoMode) ApplyFeed(); break;
            case ucDemoReset: if(demoMode) model.ResetDemo(); break;
            case ucDemoCycle: if(demoMode) model.Cycle(selected); break;
            case ucDemoUnknown: if(demoMode) for(int i=0;i<RegionCount;++i) model.states[i]=asUnknown; break;
            case ucHistory: LoadHistory(command.days); break;
            case ucExport: ExportReport(command.format); break;
            default: break;
        }
    } catch(...) { ++settingsRevision; settingsError=true; settingsMessage=L"\u041d\u0435 \u0432\u0434\u0430\u043b\u043e\u0441\u044f \u0432\u0438\u043a\u043e\u043d\u0430\u0442\u0438 \u0434\u0456\u044e. \u041f\u043e\u0432\u0442\u043e\u0440\u0456\u0442\u044c \u0441\u043f\u0440\u043e\u0431\u0443."; }
    PublishState();
}
void TMainForm::LoadHistory(int days) {
    if(reportWorker) return;
    historyDays=days; historyJson=L""; historyError=L""; historyGeneratedAt=0;
    UnicodeString script=ExtractFilePath(Application->ExeName)+L"analytics\\analyze.py";
    if(!FileExists(script)) { historyError=L"\u041d\u0435 \u0437\u043d\u0430\u0439\u0434\u0435\u043d\u043e \u043c\u043e\u0434\u0443\u043b\u044c \u0430\u043d\u0430\u043b\u0456\u0442\u0438\u043a\u0438. \u041f\u043e\u0432\u0442\u043e\u0440\u0456\u0442\u044c \u0437\u0431\u0456\u0440\u043a\u0443 \u043f\u0440\u043e\u0433\u0440\u0430\u043c\u0438."; return; }
    __int64 since=days?UtcSeconds()-(__int64)days*86400:0;
    reportWorker=new TReportWorker(script.c_str(),storage.Folder(),Regions[selected].iso,since);
    reportWorker->Start();
}
void TMainForm::PollReport() {
    if(!reportWorker || WaitForSingleObject((HANDLE)reportWorker->Handle,0)!=WAIT_OBJECT_0) return;
    unsigned long error=reportWorker->Error;
    delete reportWorker; reportWorker=0;
    if(error) { historyError=L"\u041d\u0435 \u0432\u0434\u0430\u043b\u043e\u0441\u044f \u0440\u043e\u0437\u0440\u0430\u0445\u0443\u0432\u0430\u0442\u0438 \u0456\u0441\u0442\u043e\u0440\u0456\u044e. \u041f\u0435\u0440\u0435\u0432\u0456\u0440\u0442\u0435 \u0432\u0441\u0442\u0430\u043d\u043e\u0432\u043b\u0435\u043d\u043d\u044f Python 3.10 \u0430\u0431\u043e \u043d\u043e\u0432\u0456\u0448\u043e\u0433\u043e \u0442\u0430 \u0434\u043e\u0441\u0442\u0443\u043f \u0434\u043e \u043f\u0430\u043f\u043a\u0438 \u0434\u0430\u043d\u0438\u0445."; return; }
    FILE* file=_wfopen((storage.Folder()+L"\\reports\\history.json").c_str(),L"rb");
    if(!file) { historyError=L"\u041d\u0435 \u0432\u0434\u0430\u043b\u043e\u0441\u044f \u0432\u0456\u0434\u043a\u0440\u0438\u0442\u0438 \u0440\u043e\u0437\u0440\u0430\u0445\u043e\u0432\u0430\u043d\u0443 \u0456\u0441\u0442\u043e\u0440\u0456\u044e."; return; }
    std::string body; char buffer[4096];
    while(!feof(file) && body.size()<=2*1024*1024) { unsigned count=fread(buffer,1,sizeof(buffer),file); body.append(buffer,count); if(!count) break; }
    bool valid=!ferror(file) && body.size()<=2*1024*1024; fclose(file);
    if(valid) {
        std::auto_ptr<TJSONValue> value(TJSONObject::ParseJSONValue(UTF8String(body.c_str(),body.size()),false,false));
        if(dynamic_cast<TJSONObject*>(value.get())) { historyJson=value->ToJSON(); historyGeneratedAt=UtcSeconds(); return; }
    }
    historyError=L"\u0417\u0432\u0456\u0442 \u043c\u0456\u0441\u0442\u0438\u0442\u044c \u043d\u0435\u043a\u043e\u0440\u0435\u043a\u0442\u043d\u0456 \u0434\u0430\u043d\u0456. \u0421\u043f\u0440\u043e\u0431\u0443\u0439\u0442\u0435 \u043f\u0435\u0440\u0435\u0440\u0430\u0445\u0443\u0432\u0430\u0442\u0438 \u0456\u0441\u0442\u043e\u0440\u0456\u044e.";
}
void TMainForm::ExportReport(const std::string& format) {
    if(reportWorker || historyJson.IsEmpty() || !historyError.IsEmpty()) return;
    std::auto_ptr<TSaveDialog> dialog(new TSaveDialog(this));
    UnicodeString extension(format.c_str());
    dialog->Title=L"\u0415\u043a\u0441\u043f\u043e\u0440\u0442 \u0456\u0441\u0442\u043e\u0440\u0456\u0457"; dialog->DefaultExt=extension;
    dialog->FileName=L"AirAlertMap-history."+extension;
    dialog->Filter=extension+L"|*."+extension;
    dialog->Options=dialog->Options<<ofOverwritePrompt<<ofPathMustExist;
    if(dialog->Execute()) {
        UnicodeString source=UnicodeString(storage.Folder().c_str())+L"\\reports\\history."+extension;
        ++settingsRevision;
        settingsError=CopyFileW(source.c_str(),dialog->FileName.c_str(),FALSE)==FALSE;
        settingsMessage=settingsError?UnicodeString(L"\u041d\u0435 \u0432\u0434\u0430\u043b\u043e\u0441\u044f \u0437\u0431\u0435\u0440\u0435\u0433\u0442\u0438 \u0444\u0430\u0439\u043b. \u041f\u0435\u0440\u0435\u0432\u0456\u0440\u0442\u0435 \u0434\u043e\u0441\u0442\u0443\u043f \u0434\u043e \u043f\u0430\u043f\u043a\u0438."):UnicodeString(L"\u0424\u0430\u0439\u043b \u0456\u0441\u0442\u043e\u0440\u0456\u0457 \u0437\u0431\u0435\u0440\u0435\u0436\u0435\u043d\u043e.");
    }
}
void TMainForm::StartRequest() {
    if(demoMode || worker || apiToken.empty() || authPaused || (DWORD)(GetTickCount()-lastAttemptTick)<retryDelay) return;
    lastAttemptTick=GetTickCount();
    retryDelay=60000UL;
    storage.SaveRetry(provider,60,UtcSeconds());
    try {
        worker=new TAlertWorker(apiToken,generation,provider,regionDirectory);
        worker->Start();
        connectionText=(L"\u0417\u0430\u0432\u0430\u043d\u0442\u0430\u0436\u0435\u043d\u043d\u044f \u0437 "+UnicodeString(ApiProviderName(provider))+L"\u2026");
    } catch(...) {
        delete worker; worker=0; feed.Unavailable(); ApplyFeed();
        connectionText=L"\u041d\u0435 \u0432\u0434\u0430\u043b\u043e\u0441\u044f \u0437\u0430\u043f\u0443\u0441\u0442\u0438\u0442\u0438 \u0437\u0430\u043f\u0438\u0442. \u041f\u043e\u0432\u0442\u043e\u0440 \u0447\u0435\u0440\u0435\u0437 \u0445\u0432\u0438\u043b\u0438\u043d\u0443.";
    }
    PublishState();
}
void __fastcall TMainForm::FeedTick(TObject*) {
    PollReport();
    if(worker && WaitForSingleObject((HANDLE)worker->Handle,0)==WAIT_OBJECT_0) {
        ApiResult result=worker->result;
        ApiProvider resultProvider=worker->Source();
        bool current=worker->generation==generation && !demoMode;
        delete worker; worker=0;
        lastAttemptTick=GetTickCount();
        // Respect server throttling even if the user changed mode during the request.
        retryDelay=result.retrySeconds*1000UL;
        storage.SaveRetry(resultProvider,result.retrySeconds,UtcSeconds());
        if(resultProvider!=provider) {
            unsigned remaining=storage.LoadRetry(provider,UtcSeconds());
            lastAttemptTick=GetTickCount()-(remaining?0:60000UL);
            retryDelay=remaining?remaining*1000UL:60000UL;
        }
        if(current) {
            if(!result.regionDirectory.empty()) {
                if(result.regionDirectory!=regionDirectory) storage.SaveDirectory(result.regionDirectory,UtcSeconds());
                regionDirectory=result.regionDirectory;
            }
            if(result.error==aeNone) {
                feed.Accept(result.snapshot,GetTickCount());
                confirmedAt=UtcSeconds();
                lastSuccess=FormatDateTime(L"dd.mm hh:nn:ss",Now());
                connectionText=(L"\u041e\u0442\u0440\u0438\u043c\u0430\u043d\u043e \u0434\u0430\u043d\u0456 "+UnicodeString(ApiProviderName(provider))+L". \u0427\u0430\u0441 \u0432\u0456\u0434\u043f\u043e\u0432\u0456\u0434\u0456: ")+lastSuccess+L". \u0410\u0432\u0442\u043e\u043e\u043d\u043e\u0432\u043b\u0435\u043d\u043d\u044f: 60 \u0441.";
            } else {
                feed.Unavailable();
                if(result.error==aeFormat) { regionDirectory.clear(); if(provider==apUkraineAlarm) storage.InvalidateDirectory(); }
                if(result.error==aePreparing) connectionText=L"\u0414\u043e\u0432\u0456\u0434\u043d\u0438\u043a \u0440\u0435\u0433\u0456\u043e\u043d\u0456\u0432 \u043e\u0442\u0440\u0438\u043c\u0430\u043d\u043e. \u0421\u0442\u0430\u0442\u0443\u0441\u0438 \u0437\u0430\u0432\u0430\u043d\u0442\u0430\u0436\u0430\u0442\u044c\u0441\u044f \u0447\u0435\u0440\u0435\u0437 \u0445\u0432\u0438\u043b\u0438\u043d\u0443 \u0437 \u0443\u0440\u0430\u0445\u0443\u0432\u0430\u043d\u043d\u044f\u043c \u043b\u0456\u043c\u0456\u0442\u0443 API.";
                else if(result.error==aeToken || result.error==aeForbidden) {
                    authPaused=true;
                    if(result.error==aeForbidden)
                        connectionText=L"HTTP 403: API \u0437\u0430\u0431\u043e\u0440\u043e\u043d\u0438\u0432 \u0434\u043e\u0441\u0442\u0443\u043f. \u041f\u0435\u0440\u0435\u0432\u0456\u0440\u0442\u0435 \u043f\u0440\u0430\u0432\u0430 \u043a\u043b\u044e\u0447\u0430 \u0430\u0431\u043e \u043e\u0431\u043c\u0435\u0436\u0435\u043d\u043d\u044f IP \u0443 \u043f\u043e\u0441\u0442\u0430\u0447\u0430\u043b\u044c\u043d\u0438\u043a\u0430.";
                    else if(result.status==401)
                        connectionText=(L"HTTP 401: "+UnicodeString(ApiProviderName(provider))+L" \u043d\u0435 \u043f\u0440\u0438\u0439\u043d\u044f\u0432 \u043a\u043b\u044e\u0447. \u041f\u0435\u0440\u0435\u0432\u0456\u0440\u0442\u0435 \u0432\u0438\u0431\u0440\u0430\u043d\u0438\u0439 \u0441\u0435\u0440\u0432\u0456\u0441 \u0442\u0430 \u043a\u043b\u044e\u0447.");
                    else
                        connectionText=L"\u041d\u0435\u043a\u043e\u0440\u0435\u043a\u0442\u043d\u0438\u0439 \u0444\u043e\u0440\u043c\u0430\u0442 \u043a\u043b\u044e\u0447\u0430. \u0412\u0432\u0435\u0434\u0456\u0442\u044c \u043a\u043b\u044e\u0447 \u043e\u0431\u0440\u0430\u043d\u043e\u0433\u043e \u0441\u0435\u0440\u0432\u0456\u0441\u0443.";
                } else if(result.status==429) connectionText=L"\u041b\u0456\u043c\u0456\u0442 API. \u041f\u043e\u0432\u0442\u043e\u0440 \u0447\u0435\u0440\u0435\u0437 "+IntToStr((int)result.retrySeconds)+L" \u0441; \u043f\u043e\u0442\u043e\u0447\u043d\u0456 \u0434\u0430\u043d\u0456 \u043d\u0435\u0434\u043e\u0441\u0442\u0443\u043f\u043d\u0456.";
                else if(result.error==aeFormat) connectionText=L"\u041d\u0435\u043a\u043e\u0440\u0435\u043a\u0442\u043d\u0430 \u0432\u0456\u0434\u043f\u043e\u0432\u0456\u0434\u044c API. \u0414\u0430\u043d\u0456 \u043d\u0435 \u0437\u0430\u0441\u0442\u043e\u0441\u043e\u0432\u0430\u043d\u043e; \u043f\u043e\u0432\u0442\u043e\u0440 \u0447\u0435\u0440\u0435\u0437 \u0445\u0432\u0438\u043b\u0438\u043d\u0443.";
                else if(result.error==aeStale) connectionText=L"\u0412\u0456\u0434\u043f\u043e\u0432\u0456\u0434\u044c API \u0437\u0430\u0441\u0442\u0430\u0440\u0456\u043b\u0430. \u041f\u043e\u0442\u043e\u0447\u043d\u0456 \u0434\u0430\u043d\u0456 \u043d\u0435\u0434\u043e\u0441\u0442\u0443\u043f\u043d\u0456.";
                else if(result.status) connectionText=L"\u041f\u043e\u043c\u0438\u043b\u043a\u0430 API: HTTP "+IntToStr((int)result.status)+L". \u041f\u043e\u0442\u043e\u0447\u043d\u0456 \u0434\u0430\u043d\u0456 \u043d\u0435\u0434\u043e\u0441\u0442\u0443\u043f\u043d\u0456.";
                else connectionText=L"\u041d\u0435\u043c\u0430\u0454 \u0437\u0432\u2019\u044f\u0437\u043a\u0443 \u0437 API (\u043a\u043e\u0434 "+IntToStr((int)result.networkError)+L"). \u041f\u043e\u0432\u0442\u043e\u0440 \u0447\u0435\u0440\u0435\u0437 \u0445\u0432\u0438\u043b\u0438\u043d\u0443.";
            }
            ApplyFeed();
        }

    }
    if(!demoMode) {
        if(feed.Expire(GetTickCount())) {
            connectionText=L"\u041f\u043e\u043d\u0430\u0434 90 \u0441\u0435\u043a\u0443\u043d\u0434 \u0431\u0435\u0437 \u043f\u0456\u0434\u0442\u0432\u0435\u0440\u0434\u0436\u0435\u043d\u043d\u044f API. \u041f\u043e\u0442\u043e\u0447\u043d\u0456 \u0434\u0430\u043d\u0456 \u043d\u0435\u0434\u043e\u0441\u0442\u0443\u043f\u043d\u0456.";
            ApplyFeed();
        }
        StartRequest();
    }
    PublishState();
}

void TMainForm::PublishState() {
    std::auto_ptr<TJSONObject> state(new TJSONObject());
    AlertSnapshot value;
    if(demoMode) for(int i=0;i<RegionCount;++i) { value.states[i]=model.states[i]; value.levels[i]='R'; }
    const AlertSnapshot& published=demoMode?value:feed.snapshot;
    std::string levels=PublicLevels(published,demoMode||feed.available);
    DWORD elapsed=(DWORD)(GetTickCount()-lastAttemptTick);
    unsigned remaining=elapsed<retryDelay?(retryDelay-elapsed+999)/1000:0;
    state->AddPair(L"version",new TJSONNumber(1));
    state->AddPair(L"levels",UnicodeString(levels.c_str()));
    state->AddPair(L"detailRevision",new TJSONNumber((int)detailRevision));
    if(detailsDirty) {
      TJSONArray* territories=new TJSONArray();
      if(!demoMode && feed.available) for(unsigned i=0;i<published.territories.size();++i) {
        const TerritoryStatus& t=published.territories[i];
        TJSONObject* item=new TJSONObject();
        item->AddPair(L"id",UnicodeString(t.id.c_str()));
        item->AddPair(L"parent",UnicodeString(t.parentId.c_str()));
        item->AddPair(L"name",UnicodeString(t.name.c_str()));
        item->AddPair(L"type",UnicodeString(t.type.c_str()));
        item->AddPair(L"oblast",new TJSONNumber(t.oblast));
        item->AddPair(L"level",UnicodeString(std::string(1,t.level).c_str()));
        item->AddPair(L"partial",new TJSONBool(t.partial));
        item->AddPair(L"severityKnown",new TJSONBool(t.severityKnown));
        item->AddPair(L"source",UnicodeString(t.sourceId.c_str()));
        territories->AddElement(item);
      }
      state->AddPair(L"territories",territories);
    }
    state->AddPair(L"available",new TJSONBool(feed.available && !demoMode));
    state->AddPair(L"demo",new TJSONBool(demoMode));
    state->AddPair(L"provider",UnicodeString(provider==apUkraineAlarm?L"ukrainealarm":L"alerts.in.ua"));
    state->AddPair(L"selected",new TJSONNumber(selected));
    state->AddPair(L"at",new TJSONNumber(confirmedAt));
    state->AddPair(L"now",new TJSONNumber(UtcSeconds()));
    state->AddPair(L"activeCount",new TJSONNumber(model.Count(asAlert)));
    state->AddPair(L"clearCount",new TJSONNumber(model.Count(asClear)));
    state->AddPair(L"unknownCount",new TJSONNumber(model.Count(asUnknown)));
    state->AddPair(L"connectionText",connectionText);
    state->AddPair(L"loading",new TJSONBool(worker!=0));
    state->AddPair(L"authPaused",new TJSONBool(authPaused));
    state->AddPair(L"retrySeconds",new TJSONNumber(remaining));
    state->AddPair(L"refreshAllowed",new TJSONBool(!demoMode && !worker && !apiToken.empty() && !authPaused && !remaining));
    state->AddPair(L"keyConfigured",new TJSONBool(!apiToken.empty()));
    state->AddPair(L"keySaved",new TJSONBool(keySaved));
    state->AddPair(L"remember",new TJSONBool(rememberKey));
    state->AddPair(L"settingsRevision",new TJSONNumber(settingsRevision));
    state->AddPair(L"settingsMessage",settingsMessage);
    state->AddPair(L"settingsError",new TJSONBool(settingsError));
    state->AddPair(L"historyLoading",new TJSONBool(reportWorker!=0));
    state->AddPair(L"historyError",historyError);
    state->AddPair(L"historyDays",new TJSONNumber(historyDays));
    state->AddPair(L"historyOk",new TJSONBool(historyOk));
    state->AddPair(L"historyGeneratedAt",new TJSONNumber(historyGeneratedAt));
    if(!historyJson.IsEmpty()) state->AddPair(L"history",TJSONObject::ParseJSONValue(historyJson,false,false));
    else state->AddPair(L"history",new TJSONNull());
    UpdateSnapshot(state->ToJSON());
    detailsDirty=false;
}
