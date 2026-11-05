#ifndef ALERTS_HTTP_H
#define ALERTS_HTTP_H
#include "AlertFeed.h"

enum ApiError { aeNone, aeToken, aeNetwork, aeHttp, aeFormat, aeStale, aeForbidden, aePreparing };
enum ApiProvider { apAlertsInUa, apUkraineAlarm };
const wchar_t* ApiProviderName(ApiProvider provider);
std::wstring ApiAuthorization(const std::wstring& token, ApiProvider provider);
struct ApiResult {
    AlertSnapshot snapshot;
    std::string regionDirectory;
    ApiError error;
    unsigned status;
    unsigned long networkError;
    unsigned long retrySeconds;
    ApiResult();
};
ApiResult PrepareRegionDirectory(const std::string& directory);

ApiResult DecodeApiResponse(unsigned status, const std::string& body,
                            const std::string& retryAfter, unsigned long ageSeconds,
                            ApiProvider provider = apAlertsInUa,
                            const std::string& directory = std::string());
ApiResult FetchAlerts(const std::wstring& token, ApiProvider provider = apAlertsInUa,
                      const std::string& directory = std::string());
#endif
