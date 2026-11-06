#include "../src/UiCommand.h"
#include "../src/Preferences.h"
#include <stdio.h>
#include <stdlib.h>
static int checks=0;
static void check(bool condition,const char* name) { ++checks; if(!condition) { fprintf(stderr,"FAIL: %s\n",name); exit(1); } }
int main() {
    UiCommand command;
    check(ParseUiCommand(L"{\"version\":1,\"type\":\"select\",\"region\":25}",command) && command.type==ucSelect && command.region==25,"select Kyiv city");
    check(!ParseUiCommand(L"select:25",command),"old string commands cannot bypass protocol");
    check(!ParseUiCommand(L"{\"version\":2,\"type\":\"refresh\"}",command),"unknown protocol version rejected");
    check(!ParseUiCommand(L"{\"version\":1,\"type\":\"select\",\"region\":27}",command),"region range checked");
    check(!ParseUiCommand(L"{\"version\":1,\"type\":\"select\",\"region\":\"7\"}",command),"numeric strings rejected");
    check(!ParseUiCommand(L"{\"version\":1,\"type\":\"select\",\"region\":1.5}",command),"fractional regions rejected");
    check(ParseUiCommand(L"{\"version\":1,\"type\":\"mode\",\"live\":false}",command) && !command.live,"demo mode command");
    check(!ParseUiCommand(L"{\"version\":1,\"type\":\"mode\",\"live\":\"false\"}",command),"boolean strings rejected");
    check(ParseUiCommand(L"{\"version\":1,\"type\":\"saveSettings\",\"provider\":\"ukrainealarm\",\"token\":\"fake-key\",\"remember\":true}",command) && command.token==L"fake-key" && command.remember,"new settings decoded without logging");
    check(ParseUiCommand(L"{\"version\":1,\"type\":\"saveSettings\",\"provider\":\"alerts.in.ua\",\"token\":\"\",\"remember\":false}",command) && command.token.empty() && command.provider==apAlertsInUa,"blank token can retain existing key in backend");
    check(!ParseUiCommand(L"{\"version\":1,\"type\":\"saveSettings\",\"provider\":\"other\",\"token\":\"fake\",\"remember\":true}",command),"unknown provider rejected");
    check(!ParseUiCommand(L"{\"version\":1,\"type\":\"saveSettings\",\"provider\":\"ukrainealarm\",\"token\":\"fake\\r\\nheader\",\"remember\":true}",command),"header injection rejected");
    check(!ParseUiCommand(L"{\"version\":1,\"type\":\"saveSettings\",\"provider\":\"ukrainealarm\",\"token\":\"fake\\u0000suffix\",\"remember\":true}",command),"embedded NUL cannot silently shorten a key");
    check(!ParseUiCommand(L"{\"version\":1,\"type\":\"forgetKey\"}",command),"key deletion requires explicit confirmation");
    check(ParseUiCommand(L"{\"version\":1,\"type\":\"forgetKey\",\"confirm\":true}",command) && command.type==ucForgetKey,"confirmed removal command");
    check(ParseUiCommand(L"{\"version\":1,\"type\":\"history\",\"days\":7}",command) && command.days==7,"history period decoded");
    check(!ParseUiCommand(L"{\"version\":1,\"type\":\"history\",\"days\":-1}",command),"negative period rejected");
    check(!ParseUiCommand(L"{\"version\":1,\"type\":\"history\",\"days\":9999999}",command),"unsupported period rejected");
    check(ParseUiCommand(L"{\"version\":1,\"type\":\"export\",\"format\":\"csv\"}",command) && command.format=="csv","known export format");
    check(!ParseUiCommand(L"{\"version\":1,\"type\":\"export\",\"format\":\"../../key\"}",command),"export path injection rejected");
    check(!ParseUiCommand(L"{\"version\":1,\"type\":\"execute\",\"script\":\"alert(1)\"}",command),"no arbitrary native execution");
    check(!ParseUiCommand(System::UnicodeString(std::wstring(9000,L'X').c_str()),command),"oversized message rejected");
    check(!ParseUiCommand(L"{\"version\":1,\"type\":\"select\",\"region\":0,\"region\":1}",command),"duplicate fields rejected");
    check(ParseUiCommand(L"{\"version\":1,\"type\":\"history\",\"days\":7,\"territory\":\"1293\"}",command)&&command.territory=="1293","history accepts bounded numeric territory ID");
    check(!ParseUiCommand(L"{\"version\":1,\"type\":\"history\",\"days\":7,\"territory\":\"../key\"}",command),"history ID cannot inject a path or command");
    System::UnicodeString prefs=System::UnicodeString(L"{\"version\":1,\"type\":\"preferences\",\"preferences\":")+System::UnicodeString(DefaultPreferences().c_str())+L"}";
    check(ParseUiCommand(prefs,command)&&command.type==ucPreferences&&!command.preferences.empty(),"preferences use validated native storage command");
    check(!ParseUiCommand(L"{\"version\":1,\"type\":\"preferences\",\"preferences\":{\"token\":\"secret\"}}",command),"invalid preference schema is rejected");
    check(ParseUiCommand(L"{\"version\":1,\"type\":\"retention\",\"days\":90,\"confirm\":true}",command)&&command.type==ucRetention&&command.days==90,"explicit retention command decoded");
    check(!ParseUiCommand(L"{\"version\":1,\"type\":\"retention\",\"days\":90}",command),"retention cannot run without confirmation");
    check(!ParseUiCommand(L"{\"version\":1,\"type\":\"retention\",\"days\":1,\"confirm\":true}",command),"unsupported retention rejected");
    check(ParseUiCommand(L"{\"version\":1,\"type\":\"copyDiagnostics\"}",command)&&command.type==ucCopyDiagnostics,"copy command does not accept supplied clipboard content");
    check(ParseUiCommand(L"{\"version\":1,\"type\":\"archiveHistory\"}",command)&&command.type==ucArchiveHistory,"archive command uses native destination dialog");
    printf("PASS: %d web command checks\n",checks); return 0;
}
