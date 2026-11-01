#include <vcl.h>
#pragma hdrstop
#include <tchar.h>
#include "MainForm.h"
USEFORM("MainForm.cpp", MainForm);

int WINAPI _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int) {
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
