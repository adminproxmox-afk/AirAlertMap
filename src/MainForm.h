#ifndef MAIN_FORM_H
#define MAIN_FORM_H
#include "WebMapForm.h"
#include "AlertModel.h"
#include "AppStorage.h"
#include "UiCommand.h"
#include <Vcl.ExtCtrls.hpp>
class TAlertWorker;
class TReportWorker;
class TMainForm : public TWebMapForm {
    AlertModel model;
    AlertFeed feed;
    AppStorage storage;
    TTimer* feedTimer;
    TAlertWorker* worker;
    TReportWorker* reportWorker;
    std::wstring apiToken;
    std::string regionDirectory;
    ApiProvider provider;
    bool rememberKey,keySaved,demoMode,authPaused,historyOk,settingsError,detailsDirty;
    unsigned generation,settingsRevision,detailRevision;
    DWORD lastAttemptTick,retryDelay;
    int selected,historyDays;
    __int64 confirmedAt,historyGeneratedAt;
    UnicodeString connectionText,lastSuccess,settingsMessage,historyJson,historyError;
    void __fastcall Command(TObject* sender,const UnicodeString& message);
    void __fastcall FeedTick(TObject* sender);
    void StartRequest();
    void ApplyFeed();
    void RecordObservation(bool available);
    void PublishState();
    void SaveSettings(const UiCommand& command);
    void SetMode(bool live);
    void LoadHistory(int days);
    void ExportReport(const std::string& format);
    void PollReport();
public:
    __fastcall TMainForm(TComponent* owner);
    __fastcall ~TMainForm();
};
extern PACKAGE TMainForm* MainForm;
#endif
