#ifndef RELEASE_SUPPORT_H
#define RELEASE_SUPPORT_H
#include "AlertsHttp.h"
#include <System.Classes.hpp>
struct HistoryStorageStats {
    unsigned __int64 bytes,catalogBytes;
    unsigned files,catalogs,error;
    HistoryStorageStats() : bytes(0),catalogBytes(0),files(0),catalogs(0),error(0) {}
};
HistoryStorageStats InspectHistoryStorage(const std::wstring& folder);
struct DiagnosticState {
    ApiProvider provider;
    ApiError apiError;
    unsigned httpStatus,networkError,retrySeconds,retentionDays;
    __int64 confirmedAt;
    bool available,demo,pythonAvailable,historyOk,keyConfigured;
    HistoryStorageStats storage;
    DiagnosticState() : provider(apUkraineAlarm),apiError(aeNone),httpStatus(0),networkError(0),retrySeconds(0),retentionDays(0),confirmedAt(0),available(false),demo(false),pythonAvailable(false),historyOk(true),keyConfigured(false) {}
};
System::UnicodeString BuildDiagnostics(const DiagnosticState& state);
class TStorageWorker : public TThread {
    std::wstring script,folder,destination;
    std::string action;
    unsigned days;
    __int64 now;
    HANDLE cancel;
protected:
    void __fastcall Execute();
public:
    unsigned long Error;
    std::wstring Output;
    __fastcall TStorageWorker(const std::wstring& script,const std::wstring& folder,const std::string& action,unsigned days,__int64 now,const std::wstring& destination=std::wstring());
    __fastcall ~TStorageWorker();
};
#endif
