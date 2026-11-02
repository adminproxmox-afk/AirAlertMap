#ifndef ALERT_FEED_H
#define ALERT_FEED_H
#include "AlertModel.h"
#include <string>

struct AlertSnapshot {
    unsigned char states[RegionCount];
    char levels[RegionCount]; // N = none, R = red, Y = yellow, P = partial, ? = unknown.
    AlertSnapshot();
};

bool ParseOblastResponse(const std::string& json, AlertSnapshot& result);
bool IsValidApiToken(const std::wstring& token);
unsigned long RetryDelaySeconds(unsigned status, const std::string& retryAfter);

// The UI owns this state. Worker threads only return immutable snapshots.
class AlertFeed {
public:
    AlertSnapshot snapshot;
    bool available;
    unsigned long lastConfirmedTick;
    AlertFeed();
    void Accept(const AlertSnapshot& value, unsigned long tick);
    void Unavailable();
    bool Expire(unsigned long tick);
};
#endif
