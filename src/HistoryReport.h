#ifndef HISTORY_REPORT_H
#define HISTORY_REPORT_H
#include <System.Classes.hpp>
#include <string>
std::wstring FindPythonOnPath(const std::wstring& searchPath);
class TReportWorker : public TThread {
    std::wstring script,history,output,region;
    __int64 since;
protected:
    void __fastcall Execute();
public:
    unsigned long Error;
    std::wstring Report;
    __fastcall TReportWorker(const std::wstring& scriptPath,const std::wstring& folder,const std::string& iso,__int64 periodStart=0);
};
#endif
