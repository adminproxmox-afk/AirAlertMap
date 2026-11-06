#ifndef PREFERENCES_H
#define PREFERENCES_H
#include <string>
std::string DefaultPreferences();
bool IsTerritoryKey(const std::string& key);
bool NormalizePreferences(const std::string& json,std::string& normalized);
#endif
