#include "ApiCredentials.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincred.h>
#include <vector>
#include <wchar.h>
#pragma link "advapi32.lib"

const wchar_t* const ApiCredentialTarget=L"AirAlertMap/API";

static bool ValidTarget(const wchar_t* target) {
    if(target && *target && wcslen(target)<=CRED_MAX_GENERIC_TARGET_NAME_LENGTH) return true;
    SetLastError(ERROR_INVALID_PARAMETER);
    return false;
}
bool SaveApiCredential(const wchar_t* target,const std::wstring& token,ApiProvider provider) {
    if(!ValidTarget(target)) return false;
    if(!IsValidApiToken(token) || token.size()>CRED_MAX_CREDENTIAL_BLOB_SIZE ||
       (provider!=apUkraineAlarm && provider!=apAlertsInUa)) {
        SetLastError(ERROR_INVALID_DATA); return false;
    }
    // API tokens are ASCII. Store one byte per character, within WinCred limits.
    std::vector<BYTE> blob(token.size());
    for(unsigned i=0;i<token.size();++i) blob[i]=(BYTE)token[i];
    CREDENTIALW entry={0};
    entry.Type=CRED_TYPE_GENERIC;
    entry.TargetName=const_cast<LPWSTR>(target);
    entry.UserName=const_cast<LPWSTR>(provider==apUkraineAlarm?L"UkraineAlarm":L"alerts.in.ua");
    entry.CredentialBlobSize=blob.size();
    entry.CredentialBlob=&blob[0];
    entry.Persist=CRED_PERSIST_LOCAL_MACHINE;
    BOOL saved=CredWriteW(&entry,0);
    DWORD error=saved?ERROR_SUCCESS:GetLastError();
    SecureZeroMemory(&blob[0],blob.size());
    SetLastError(error);
    return saved!=FALSE;
}
struct CredentialBuffer {
    PCREDENTIALW value;
    CredentialBuffer() : value(NULL) {}
    ~CredentialBuffer() {
        if(value) {
            if(value->CredentialBlob) SecureZeroMemory(value->CredentialBlob,value->CredentialBlobSize);
            CredFree(value);
        }
    }
};
bool LoadApiCredential(const wchar_t* target,std::wstring& token,ApiProvider& provider) {
    if(!ValidTarget(target)) return false;
    CredentialBuffer entry;
    if(!CredReadW(target,CRED_TYPE_GENERIC,0,&entry.value)) return false;
    if(!entry.value->UserName || !entry.value->CredentialBlob ||
       !entry.value->CredentialBlobSize || entry.value->CredentialBlobSize>2048) {
        SetLastError(ERROR_INVALID_DATA); return false;
    }
    ApiProvider selected;
    if(wcscmp(entry.value->UserName,L"UkraineAlarm")==0) selected=apUkraineAlarm;
    else if(wcscmp(entry.value->UserName,L"alerts.in.ua")==0) selected=apAlertsInUa;
    else { SetLastError(ERROR_INVALID_DATA); return false; }
    std::wstring restored;
    for(DWORD i=0;i<entry.value->CredentialBlobSize;++i) restored+=(wchar_t)entry.value->CredentialBlob[i];
    if(!IsValidApiToken(restored)) { SetLastError(ERROR_INVALID_DATA); return false; }
    token=restored; provider=selected;
    return true;
}
bool DeleteApiCredential(const wchar_t* target) {
    if(!ValidTarget(target)) return false;
    if(CredDeleteW(target,CRED_TYPE_GENERIC,0)) return true;
    return GetLastError()==ERROR_NOT_FOUND;
}
