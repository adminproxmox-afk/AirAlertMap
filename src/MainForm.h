#ifndef MAIN_FORM_H
#define MAIN_FORM_H
#include <Vcl.Forms.hpp>
#include <Vcl.StdCtrls.hpp>
#include <Vcl.ExtCtrls.hpp>
#include <vector>
#include "AlertModel.h"
#include "AlertFeed.h"
#include "AlertsHttp.h"
#include "AppStorage.h"

class TAlertWorker;
class TReportWorker;
class TWebMapForm;

class TMainForm : public TForm {
__published:
    void __fastcall PaintView(TObject* Sender);
    void __fastcall ResizeView(TObject* Sender);
    void __fastcall SelectRegion(TObject* Sender);
    void __fastcall ChangeState(TObject* Sender);
    void __fastcall ResetDemo(TObject* Sender);
    void __fastcall MapMouseDown(TObject* Sender, TMouseButton Button,
                                TShiftState Shift, int X, int Y);
    void __fastcall MapMouseMove(TObject* Sender, TShiftState Shift, int X, int Y);
    void __fastcall KeyPressed(TObject* Sender, WORD &Key, TShiftState Shift);
private:
    AlertModel model;
    TComboBox* regionBox;
    TButton* changeButton;
    TButton* resetButton;
    TButton* clearButton;
    TCheckBox* liveBox;
    TButton* tokenButton;
    TButton* refreshButton;
    TButton* webMapButton;
    TButton* historyButton;
    TWebMapForm* webMap;
    TReportWorker* reportWorker;
    TTimer* feedTimer;
    TAlertWorker* worker;
    AlertFeed feed;
    AppStorage storage;
    std::wstring apiToken;
    std::string regionDirectory;
    ApiProvider provider;
    bool rememberKey;
    bool demoMode;
    bool authPaused;
    unsigned generation;
    DWORD lastAttemptTick;
    DWORD retryDelay;
    UnicodeString connectionText;
    UnicodeString lastSuccess;
    __int64 confirmedAt;
    bool historyOk;
    int selected;
    UnicodeString changedAt;
    std::vector<HRGN> hitRegions;
    TRect mapRect;
    void DrawTextAt(int x, int y, const UnicodeString& text, int size, TColor color, bool bold = false);
    void Box(const TRect& rect, TColor fill, TColor border, int radius = 16);
    void Dot(int x, int y, TColor color, int radius = 4);
    TColor StateColor(int state) const;
    TColor RegionColor(int region, bool fill = false) const;
    UnicodeString StateText(int state) const;
    void ClearRegions();
    int HitTest(int x, int y) const;
    void __fastcall UnknownData(TObject* Sender);
    void __fastcall ModeChanged(TObject* Sender);
    void __fastcall EditToken(TObject* Sender);
    void __fastcall RefreshFeed(TObject* Sender);
    void __fastcall FeedTick(TObject* Sender);
    void StartRequest();
    void ApplyFeed();
    void UpdateControls();
    void RecordObservation(bool available);
    void UpdateWebMap();
    void __fastcall OpenWebMap(TObject* Sender);
    void __fastcall OpenHistory(TObject* Sender);
    void __fastcall WebRegionSelected(TObject* Sender);
    UnicodeString RegionStatusText(int region) const;
public:
    __fastcall TMainForm(TComponent* Owner);
    __fastcall ~TMainForm();
};
extern PACKAGE TMainForm* MainForm;
#endif
