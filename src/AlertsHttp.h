#ifndef ALERTS_HTTP_H
#define ALERTS_HTTP_H
#include "AlertFeed.h"

enum ApiError { aeNone, aeToken, aeNetwork, aeHttp, aeFormat, aeStale };
struct ApiResult {
    AlertSnapshot snapshot;
    ApiError error;
    unsigned status;
    unsigned long networkError;
    unsigned long retrySeconds;
    ApiResult();
};

ApiResult DecodeApiResponse(unsigned status, const std::string& body,
                            const std::string& retryAfter, unsigned long ageSeconds);
ApiResult FetchAlerts(const std::wstring& token);
#endif
