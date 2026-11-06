#ifndef HISTORY_REPORT_H
#define HISTORY_REPORT_H
#include <System.Classes.hpp>
#include <string>
#include <vector>
std::wstring FindPythonOnPath(const std::wstring& searchPath);
bool PythonAvailable();
unsigned long RunPythonTool(const std::wstring& script,const std::vector<std::wstring>& args,unsigned long timeoutMs=120000,HANDLE cancel=NULL);
class TReportWorker : public TThread {
    std::wstring script,history,output,region,territory;
    __int64 since;
protected:
    void __fastcall Execute();
public:
    unsigned long Error;
    std::wstring Report;
    __fastcall TReportWorker(const std::wstring& scriptPath,const std::wstring& folder,const std::string& iso,__int64 periodStart=0,const std::string& territoryId=std::string());
};
#endif
