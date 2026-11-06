#ifndef UI_COMMAND_H
#define UI_COMMAND_H
#include "AlertsHttp.h"
#include <System.hpp>
enum UiCommandType { ucInvalid,ucReady,ucSelect,ucRefresh,ucMode,ucSaveSettings,ucForgetKey,ucDemoReset,ucDemoCycle,ucDemoUnknown,ucHistory,ucExport };
struct UiCommand {
    UiCommandType type;
    int region,days;
    ApiProvider provider;
    bool remember,live;
    std::wstring token;
    std::string format;
    UiCommand() : type(ucInvalid),region(-1),days(0),provider(apUkraineAlarm),remember(false),live(true) {}
};
bool ParseUiCommand(const System::UnicodeString& json,UiCommand& command);
#endif
