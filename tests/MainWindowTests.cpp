#include <vcl.h>
#include <stdio.h>
#include "../src/WebMapForm.h"
#pragma resource "../tests/MainWindowTests.dfm"
static bool constructed=false;
static bool independentAtConstruction=false;
class TWindowProbe : public TWebMapForm {
public:
    __fastcall TWindowProbe(TComponent* owner);
    void RecreateForTest() { RecreateWnd(); }
};
__fastcall TWindowProbe::TWindowProbe(TComponent* owner) : TWebMapForm(owner,
  ExtractFilePath(Application->ExeName)+L"web\\index.html",
  ExtractFilePath(Application->ExeName)+L"WindowTestProfile") {
    constructed=true; HandleNeeded();
    independentAtConstruction=GetWindow(Handle,GW_OWNER)==NULL;
}
int main() {
    Application->Initialize(); Application->MainFormOnTaskBar=true;
    TWindowProbe* form=0;
    Application->CreateForm(__classid(TWindowProbe),&form);
    bool registered=Application->MainForm==form;
    bool independent=GetWindow(form->Handle,GW_OWNER)==NULL;
    bool taskbarIcon=SendMessage(form->Handle,WM_GETICON,ICON_BIG,0)!=0;
    bool titleIcon=SendMessage(form->Handle,WM_GETICON,ICON_SMALL,0)!=0;
    form->RecreateForTest();
    bool recreatedIcons=SendMessage(form->Handle,WM_GETICON,ICON_BIG,0)!=0 && SendMessage(form->Handle,WM_GETICON,ICON_SMALL,0)!=0;
    printf("Window registration: %s, ShowMainForm: %s\n",registered?"matched":"FAILED",Application->ShowMainForm?"true":"false");
    delete form;
    if(!registered || !constructed) { fprintf(stderr,"FAIL: main form constructor or registration\n"); return 1; }
    if(!independent || !independentAtConstruction) { fprintf(stderr,"FAIL: main window is owned by hidden application window\n"); return 1; }
    if(!taskbarIcon || !titleIcon || !recreatedIcons) { fprintf(stderr,"FAIL: main window needs explicit taskbar/title icons, including after handle recreation\n"); return 1; }
    puts("PASS: registered main form has taskbar/title icons before and after handle recreation"); return 0;
}
