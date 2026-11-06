#ifndef MAIN_FORM_H
#define MAIN_FORM_H
#include "WebMapForm.h"
#include "AlertModel.h"
#include "AppStorage.h"
#include "UiCommand.h"
#include "ReleaseSupport.h"
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
    TStorageWorker* storageWorker;
    HistoryStorageStats historyStorage;
    unsigned storageRevision,diagnosticsRevision,retentionDays;
    ApiError lastApiError;
    unsigned lastHttpStatus,lastNetworkError;
    __int64 lastCleanupAttempt;
    bool storageError,diagnosticsError;
    std::string storageAction;
    UnicodeString storageMessage,diagnosticsMessage,diagnostics;
    std::wstring apiToken;
    std::string regionDirectory;
    std::string preferences,historyTerritory,pendingTerritory;
    ApiProvider provider;
    bool rememberKey,keySaved,demoMode,authPaused,historyOk,settingsError,detailsDirty,pythonAvailable,historyPending;
    unsigned generation,settingsRevision,detailRevision,preferencesRevision;
    DWORD lastAttemptTick,retryDelay;
    int selected,historyDays,pendingDays;
    __int64 confirmedAt,historyGeneratedAt;
    UnicodeString connectionText,lastSuccess,settingsMessage,historyJson,historyError,preferencesError;
    void __fastcall Command(TObject* sender,const UnicodeString& message);
    void __fastcall FeedTick(TObject* sender);
    void StartRequest();
    void ApplyFeed();
    void RecordObservation(bool available);
    void PublishState();
    void SaveSettings(const UiCommand& command);
    void SetMode(bool live);
    void LoadHistory(int days,const std::string& territory=std::string());
    void ExportReport(const std::string& format);
    void PollReport();
    void RefreshStorage(bool preserveMessage=false);
    void StartStorage(const std::string& action,const std::wstring& destination=std::wstring());
    void PollStorage();
    void ChangeRetention(unsigned days);
    void ArchiveHistory();
    void CopyDiagnostics();
public:
    __fastcall TMainForm(TComponent* owner);
    __fastcall ~TMainForm();
};
extern PACKAGE TMainForm* MainForm;
#endif
