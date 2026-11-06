#ifndef APP_STORAGE_H
#define APP_STORAGE_H
#include "AlertsHttp.h"

__int64 UtcSeconds();
std::string PublicLevels(const AlertSnapshot& snapshot,bool available);
class AppStorage {
    std::wstring root;
    bool Read(const wchar_t* name,std::string& body,unsigned maximum) const;
    bool Write(const wchar_t* name,const std::string& body) const;
    bool SaveCatalog(const std::vector<TerritoryStatus>& territories,std::string& digest) const;
public:
    explicit AppStorage(const std::wstring& folder=std::wstring());
    const std::wstring& Folder() const { return root; }
    bool SaveDirectory(const std::string& body,__int64 now) const;
    bool LoadDirectory(std::string& body,__int64 now) const;
    void InvalidateDirectory() const;
    bool SaveSelectedRegion(int index) const;
    int LoadSelectedRegion(int fallback=7) const;
    bool SavePreferences(const std::string& json) const;
    bool LoadPreferences(std::string& json) const;
    bool SaveHistoryRetention(unsigned days) const;
    unsigned LoadHistoryRetention() const;
    bool SaveRetry(ApiProvider provider,unsigned seconds,__int64 now) const;
    unsigned LoadRetry(ApiProvider provider,__int64 now) const;
    bool AppendObservation(const AlertSnapshot& snapshot,bool available,ApiProvider provider,__int64 now) const;
};
#endif
