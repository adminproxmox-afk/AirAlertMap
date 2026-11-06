#include <vcl.h>
#include <stdio.h>
#include "../src/WebMapForm.h"
#pragma resource "../tests/MainWindowTests.dfm"
static bool constructed=false;
static bool independentAtConstruction=false;
class TWindowProbe : public TWebMapForm {
public:
    __fastcall TWindowProbe(TComponent* owner);
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
    printf("Window registration: %s, ShowMainForm: %s\n",registered?"matched":"FAILED",Application->ShowMainForm?"true":"false");
    delete form;
    if(!registered || !constructed) { fprintf(stderr,"FAIL: main form constructor or registration\n"); return 1; }
    if(!independent || !independentAtConstruction) { fprintf(stderr,"FAIL: main window is owned by hidden application window\n"); return 1; }
    puts("PASS: web window is registered as application main form"); return 0;
}
