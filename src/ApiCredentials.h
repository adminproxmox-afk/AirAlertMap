#ifndef API_CREDENTIALS_H
#define API_CREDENTIALS_H
#include "AlertsHttp.h"

extern const wchar_t* const ApiCredentialTarget;
// Generic Windows credentials; the target namespaces this application's entry.
bool SaveApiCredential(const wchar_t* target,const std::wstring& token,ApiProvider provider);
bool LoadApiCredential(const wchar_t* target,std::wstring& token,ApiProvider& provider);
bool DeleteApiCredential(const wchar_t* target);
#endif
