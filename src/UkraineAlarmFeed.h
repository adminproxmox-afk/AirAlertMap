#ifndef UKRAINE_ALARM_FEED_H
#define UKRAINE_ALARM_FEED_H
#include "AlertFeed.h"
bool ParseUkraineAlarmCoverage(const std::string& json, const std::string& directory, AlertSnapshot& result);
std::string PublicUkraineAlarmDiagnostic(const std::string& json);
#endif
