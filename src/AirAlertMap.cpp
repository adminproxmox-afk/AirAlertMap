#include <vcl.h>
#pragma hdrstop
#include <tchar.h>
#include "MainForm.h"
#include "SingleInstance.h"
USEFORM("MainForm.cpp", MainForm);

int WINAPI _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int) {
    SingleInstanceGuard instance;
    if(!instance.IsFirst()&&!instance.Error())instance.ActivateExisting();
    if(!instance.IsFirst()) {
        if(instance.Error())MessageBoxW(NULL,L"\u041d\u0435 \u0432\u0434\u0430\u043b\u043e\u0441\u044f \u0432\u0456\u0434\u043a\u0440\u0438\u0442\u0438 \u0432\u0436\u0435 \u0437\u0430\u043f\u0443\u0449\u0435\u043d\u0435 \u0432\u0456\u043a\u043d\u043e AirAlertMap. \u041f\u0435\u0440\u0435\u0432\u0456\u0440\u0442\u0435, \u0447\u0438 \u0432\u0456\u0434\u043f\u043e\u0432\u0456\u0434\u0430\u0454 \u043f\u043e\u0442\u043e\u0447\u043d\u0430 \u043a\u043e\u043f\u0456\u044f.",L"AirAlertMap",MB_OK|MB_ICONINFORMATION);
        return instance.Error()?1:0;
    }
    try {
        Application->Initialize();
        Application->MainFormOnTaskBar = true;
        Application->Title = L"\u041c\u0430\u043f\u0430 \u0442\u0440\u0438\u0432\u043e\u0433 | AirAlertMap";
        Application->CreateForm(__classid(TMainForm), &MainForm);
        Application->Run();
    } catch (Exception &exception) {
        Application->ShowException(&exception);
        return 1;
    }
    return 0;
}
