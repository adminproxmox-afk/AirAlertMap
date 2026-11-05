#include <vcl.h>
#pragma hdrstop
#include "MainForm.h"
#include "MapData.h"
#include "AlertsHttp.h"
#include "ApiCredentials.h"
#include <math.h>
#pragma package(smart_init)
#pragma resource "MainForm.dfm"

TMainForm* MainForm;

// No VCL calls or Synchronize in the worker: the UI polls its OS thread handle.
class TAlertWorker : public TThread {
    std::wstring token;
    std::string directory;
    ApiProvider provider;
protected:
    void __fastcall Execute() {
        try { result=FetchAlerts(token,provider,directory); }
        catch(...) { result=ApiResult(); result.networkError=ERROR_GEN_FAILURE; }
        token.clear();
    }
public:
    ApiResult result;
    unsigned generation;
    __fastcall TAlertWorker(const std::wstring& value,unsigned version,ApiProvider source,const std::string& schema)
      : TThread(true),token(value),directory(schema),provider(source),generation(version) { FreeOnTerminate=false; }
};
static TColor RGBColor(int r, int g, int b) { return (TColor)RGB(r,g,b); }
static const TColor Background = (TColor)0x17110D;
static const TColor Panel = (TColor)0x231B15;
static const TColor Border = (TColor)0x3B3026;
static const TColor White = (TColor)0xF4F0EB;
static const TColor Muted = (TColor)0xAA9B8D;
static const TColor Accent = (TColor)0xDCD557;

__fastcall TMainForm::TMainForm(TComponent* Owner) : TForm(Owner), regionBox(0), changeButton(0), resetButton(0), clearButton(0), liveBox(0), tokenButton(0), refreshButton(0), feedTimer(0), worker(0), provider(apUkraineAlarm), rememberKey(true), demoMode(false), authPaused(false), generation(0), lastAttemptTick(GetTickCount()-60000UL), retryDelay(60000UL), selected(7) {
    Caption = L"\u041c\u0430\u043f\u0430 \u0442\u0440\u0438\u0432\u043e\u0433 \u0423\u043a\u0440\u0430\u0457\u043d\u0438 | API + C++Builder + x86 ASM";
    Color = Background;
    Font->Name = L"Segoe UI";
    Font->Size = 10;
    Font->Color = White;
    Position = poScreenCenter;
    ClientWidth = 1280;
    ClientHeight = 850;
    Constraints->MinWidth = 1120;
    Constraints->MinHeight = 860;
    DoubleBuffered = true;
    KeyPreview = true;
    OnPaint = PaintView;
    OnResize = ResizeView;
    OnMouseDown = MapMouseDown;
    OnMouseMove = MapMouseMove;
    OnKeyDown = KeyPressed;

    regionBox = new TComboBox(this);
    regionBox->Parent = this;
    regionBox->Style = csDropDownList;
    regionBox->Color = RGBColor(29,40,50);
    regionBox->Font->Color = White;
    regionBox->Font->Size = 10;
    for (int i=0;i<RegionCount;++i) regionBox->Items->Add(Regions[i].name);
    regionBox->ItemIndex = selected;
    regionBox->OnChange = SelectRegion;
    regionBox->TabOrder = 0;

    changeButton = new TButton(this);
    changeButton->Parent = this;
    changeButton->Caption = L"\u0417\u043c\u0456\u043d\u0438\u0442\u0438 \u0434\u0435\u043c\u043e\u0441\u0442\u0430\u0442\u0443\u0441";
    changeButton->OnClick = ChangeState;
    changeButton->TabOrder = 1;
    resetButton = new TButton(this);
    resetButton->Parent = this;
    resetButton->Caption = L"\u0412\u0456\u0434\u043d\u043e\u0432\u0438\u0442\u0438 \u0434\u0435\u043c\u043e\u0441\u0446\u0435\u043d\u0430\u0440\u0456\u0439";
    resetButton->OnClick = ResetDemo;
    resetButton->TabOrder = 2;
    clearButton = new TButton(this);
    clearButton->Parent = this;
    clearButton->Caption = L"\u041f\u043e\u0437\u043d\u0430\u0447\u0438\u0442\u0438 \u0432\u0441\u0456: \u043d\u0435\u043c\u0430\u0454 \u0434\u0430\u043d\u0438\u0445";
    clearButton->OnClick = UnknownData;
    clearButton->TabOrder = 3;
    liveBox = new TCheckBox(this);
    liveBox->Parent = this;
    liveBox->Caption = L"\u0410\u043a\u0442\u0443\u0430\u043b\u044c\u043d\u0456 \u0434\u0430\u043d\u0456 (API)";
    liveBox->Font->Color = White;
    liveBox->Checked = true;
    liveBox->OnClick = ModeChanged;
    liveBox->TabOrder = 1;
    tokenButton = new TButton(this);
    tokenButton->Parent = this;
    tokenButton->Caption = L"\u041d\u0430\u043b\u0430\u0448\u0442\u0443\u0432\u0430\u0442\u0438 API-\u0442\u043e\u043a\u0435\u043d";
    tokenButton->OnClick = EditToken;
    tokenButton->TabOrder = 2;
    refreshButton = new TButton(this);
    refreshButton->Parent = this;
    refreshButton->Caption = L"\u041e\u043d\u043e\u0432\u0438\u0442\u0438 \u0437\u0430\u0440\u0430\u0437";
    refreshButton->OnClick = RefreshFeed;
    refreshButton->TabOrder = 3;
    if(!LoadApiCredential(ApiCredentialTarget,apiToken,provider)) {
        apiToken=System::Sysutils::GetEnvironmentVariable(L"UKRAINEALARM_API_KEY").c_str();
        if(apiToken.empty()) {
            apiToken=System::Sysutils::GetEnvironmentVariable(L"ALERTS_IN_UA_TOKEN").c_str();
            if(!apiToken.empty()) provider=apAlertsInUa;
        }
    }
    if(!IsValidApiToken(apiToken)) apiToken.clear();
    connectionText = apiToken.empty()?L"\u0414\u043e\u0434\u0430\u0439\u0442\u0435 API-\u0442\u043e\u043a\u0435\u043d \u2014 \u0434\u0430\u043d\u0456 \u043f\u043e\u043a\u0438 \u043d\u0435\u0434\u043e\u0441\u0442\u0443\u043f\u043d\u0456.":L"\u041e\u0447\u0456\u043a\u0443\u0432\u0430\u043d\u043d\u044f \u043f\u0435\u0440\u0448\u043e\u0433\u043e \u0437\u0430\u043f\u0438\u0442\u0443 \u0434\u043e API\u2026";
    lastSuccess=L"\u0449\u0435 \u043d\u0435 \u043e\u0442\u0440\u0438\u043c\u0430\u043d\u043e";
    feedTimer = new TTimer(this);
    feedTimer->Interval = 250;
    feedTimer->OnTimer = FeedTick;
    feedTimer->Enabled = true;
    ResizeView(this);
    UpdateControls();
}

__fastcall TMainForm::~TMainForm() {
    if(feedTimer) feedTimer->Enabled=false;
    // HTTP has finite timeouts; no callback can outlive this form.
    if(worker) { worker->WaitFor(); delete worker; worker=0; }
    ClearRegions();
}
void TMainForm::ClearRegions() {
    for (unsigned i=0;i<hitRegions.size();++i) if (hitRegions[i]) DeleteObject(hitRegions[i]);
    hitRegions.clear();
}

void TMainForm::DrawTextAt(int x, int y, const UnicodeString& text, int size, TColor color, bool bold) {
    Canvas->Font->Name = L"Segoe UI";
    Canvas->Font->Size = size;
    Canvas->Font->Color = color;
    Canvas->Font->Style = bold ? TFontStyles() << fsBold : TFontStyles();
    Canvas->Brush->Style = bsClear;
    Canvas->TextOut(x,y,text);
    Canvas->Brush->Style = bsSolid;
}
void TMainForm::Box(const TRect& r, TColor fill, TColor border, int radius) {
    Canvas->Brush->Color = fill;
    Canvas->Pen->Color = border;
    Canvas->Pen->Width = 1;
    Canvas->RoundRect(r.Left,r.Top,r.Right,r.Bottom,radius,radius);
}
void TMainForm::Dot(int x,int y,TColor color,int radius) {
    Canvas->Brush->Color = color;
    Canvas->Pen->Color = color;
    Canvas->Ellipse(x-radius,y-radius,x+radius,y+radius);
}
TColor TMainForm::StateColor(int state) const {
    if(state==asAlert) return RGBColor(246,106,112);
    if(state==asClear) return RGBColor(78,186,164);
    return RGBColor(135,150,169);
}
TColor TMainForm::RegionColor(int region,bool fill) const {
    char level=demoMode?'R':feed.snapshot.levels[region];
    AlertVisual visual=VisualForState(model.states[region],level);
    if(visual==avAmber) return fill?RGBColor(119,91,35):RGBColor(244,191,77);
    if(visual==avRed) return fill?RGBColor(124,50,62):StateColor(asAlert);
    if(visual==avClear) return fill?RGBColor(32,74,73):StateColor(asClear);
    return fill?RGBColor(49,59,74):StateColor(asUnknown);
}
UnicodeString TMainForm::StateText(int state) const {
    if(state==asAlert) return L"\u041f\u043e\u0432\u0456\u0442\u0440\u044f\u043d\u0430 \u0442\u0440\u0438\u0432\u043e\u0433\u0430";
    if(state==asClear) return demoMode?L"\u0411\u0435\u0437 \u0442\u0440\u0438\u0432\u043e\u0433\u0438 (\u0434\u0435\u043c\u043e)":L"\u041d\u0435\u043c\u0430\u0454 \u0430\u043a\u0442\u0438\u0432\u043d\u043e\u0457 \u0442\u0440\u0438\u0432\u043e\u0433\u0438";
    return L"\u041d\u0435\u043c\u0430\u0454 \u0434\u0430\u043d\u0438\u0445";
}

void __fastcall TMainForm::ResizeView(TObject*) {
    if (!regionBox || !liveBox) return;
    const int sx=ClientWidth-304;
    regionBox->SetBounds(sx+20,286,244,30);
    liveBox->SetBounds(sx+20,440,244,24);
    tokenButton->SetBounds(sx+20,478,244,34);
    refreshButton->SetBounds(sx+20,522,244,34);
    changeButton->SetBounds(sx+20,478,244,34);
    resetButton->SetBounds(sx+20,522,244,34);
    clearButton->SetBounds(sx+20,566,244,30);
    Invalidate();
}

void __fastcall TMainForm::PaintView(TObject*) {
    const int w=ClientWidth, h=ClientHeight, sx=w-304;
    Canvas->Brush->Color = Background;
    Canvas->FillRect(ClientRect);
    Box(Rect(24,26,66,68),RGBColor(27,57,61),RGBColor(49,89,90),12);
    Canvas->Pen->Color = Accent; Canvas->Pen->Width=2;
    Canvas->MoveTo(34,54); Canvas->LineTo(41,40); Canvas->LineTo(47,54); Canvas->LineTo(56,37);
    Canvas->Pen->Width=1;
    DrawTextAt(82,22,L"\u041c\u0430\u043f\u0430 \u0442\u0440\u0438\u0432\u043e\u0433",24,White,true);
    DrawTextAt(83,62,L"\u0423\u041a\u0420\u0410\u0407\u041d\u0410  /  \u041f\u041e\u0412\u0406\u0422\u0420\u042f\u041d\u0410 \u0411\u0415\u0417\u041f\u0415\u041a\u0410",9,Muted);
    TColor badge=demoMode?RGBColor(240,190,104):feed.available?StateColor(asClear):StateColor(asUnknown);
    Box(Rect(w-310,30,w-24,66),Panel,Border,16);
    Dot(w-290,48,badge);
    DrawTextAt(w-275,37,demoMode?L"\u0414\u0415\u041c\u041e \u00b7 \u0412\u0418\u0413\u0410\u0414\u0410\u041d\u0406 \u0414\u0410\u041d\u0406":feed.available?L"API \u00b7 \u0414\u0410\u041d\u0406 \u041e\u0422\u0420\u0418\u041c\u0410\u041d\u041e":L"API \u00b7 \u041d\u0415\u041c\u0410\u0404 \u0414\u0410\u041d\u0418\u0425",9,badge,true);
    Canvas->Pen->Color=Border; Canvas->MoveTo(24,101); Canvas->LineTo(w-24,101);

    const int cardWidth=(w-72)/3;
    const int states[3]={asAlert,asClear,asUnknown};
    const wchar_t* captions[3]={L"\u0420\u0435\u0433\u0456\u043e\u043d\u0456\u0432 \u0456\u0437 \u0442\u0440\u0438\u0432\u043e\u0433\u043e\u044e / \u0437\u0430\u0433\u0440\u043e\u0437\u043e\u044e",L"\u0420\u0435\u0433\u0456\u043e\u043d\u0456\u0432 \u0431\u0435\u0437 \u0442\u0440\u0438\u0432\u043e\u0433\u0438",L"\u041d\u0435\u043c\u0430\u0454 \u0434\u0430\u043d\u0438\u0445"};
    for(int i=0;i<3;++i) {
        const int x=24+i*(cardWidth+12);
        Box(Rect(x,122,x+cardWidth,209),Panel,Border);
        Dot(x+22,145,StateColor(states[i]));
        DrawTextAt(x+36,135,captions[i],10,Muted);
        DrawTextAt(x+20,156,IntToStr(model.Count((AlertState)states[i])),25,White,true);
        DrawTextAt(x+79,175,L"\u0456\u0437 27 \u0440\u0435\u0433\u0456\u043e\u043d\u0456\u0432",9,Muted);
    }

    mapRect=Rect(24,230,sx-16,h-100);
    Box(mapRect,Panel,Border);
    DrawTextAt(44,248,L"\u041e\u0433\u043b\u044f\u0434 \u0440\u0435\u0433\u0456\u043e\u043d\u0456\u0432",13,White,true);
    DrawTextAt(44,278,L"\u041e\u0431\u0435\u0440\u0456\u0442\u044c \u043e\u0431\u043b\u0430\u0441\u0442\u044c \u043d\u0430 \u043c\u0430\u043f\u0456 \u0430\u0431\u043e \u0443 \u0441\u043f\u0438\u0441\u043a\u0443 \u043f\u0440\u0430\u0432\u043e\u0440\u0443\u0447",9,Muted);
    const TRect plot=Rect(mapRect.Left+28,mapRect.Top+86,mapRect.Right-28,mapRect.Bottom-52);
    const double scaleX=(plot.Right-plot.Left)/(MapMaxX-MapMinX);
    const double scaleY=(plot.Bottom-plot.Top)/(MapMaxY-MapMinY);
    const double scale=scaleX<scaleY?scaleX:scaleY;
    const double ox=plot.Left+((plot.Right-plot.Left)-(MapMaxX-MapMinX)*scale)/2;
    const double oy=plot.Top+((plot.Bottom-plot.Top)-(MapMaxY-MapMinY)*scale)/2;

    // Build one even-odd region for each administrative unit, preserving holes.
    ClearRegions();
    for(int i=0;i<RegionCount;++i) {
        std::vector<POINT> pts; std::vector<int> counts;
        for(int r=0;r<MapRingCount;++r) if(MapRings[r].region==i) {
            const MapRing& ring=MapRings[r]; counts.push_back(ring.count);
            for(int j=0;j<ring.count;++j) {
                POINT p;
                p.x=(LONG)(ox+(ring.points[j].x-MapMinX)*scale+0.5);
                p.y=(LONG)(oy+(ring.points[j].y-MapMinY)*scale+0.5);
                pts.push_back(p);
            }
        }
        HRGN hrgn=pts.empty()?CreateRectRgn(0,0,0,0):CreatePolyPolygonRgn(&pts[0],&counts[0],counts.size(),ALTERNATE);
        hitRegions.push_back(hrgn);
        TColor fill=RegionColor(i,true);
        HBRUSH brush=CreateSolidBrush(ColorToRGB(fill));
        FillRgn(Canvas->Handle,hrgn,brush); DeleteObject(brush);
        brush=CreateSolidBrush(ColorToRGB(RGBColor(85,111,119)));
        FrameRgn(Canvas->Handle,hrgn,brush,1,1); DeleteObject(brush);
    }
    if(selected>=0 && selected<(int)hitRegions.size()) {
        HBRUSH brush=CreateSolidBrush(ColorToRGB(Accent));
        FrameRgn(Canvas->Handle,hitRegions[selected],brush,2,2); DeleteObject(brush);
    }
    // Label major regions; the two city units remain selectable via the list.
    for(int i=0;i<25;++i) {
        int x=(int)(ox+(Regions[i].labelX-MapMinX)*scale);
        int y=(int)(oy+(Regions[i].labelY-MapMinY)*scale);
        Canvas->Font->Name=L"Segoe UI"; Canvas->Font->Size=8;
        Canvas->Font->Style=TFontStyles();
        UnicodeString label=Regions[i].shortName;
        DrawTextAt(x-Canvas->TextWidth(label)/2,y-7,label,8,i==selected?Accent:White);
    }
    DrawTextAt(mapRect.Left+22,mapRect.Bottom-29,L"\u041c\u0435\u0436\u0456: geoBoundaries / \u00a9 OpenStreetMap contributors \u00b7 ODbL 1.0",8,Muted);

    Box(Rect(sx,230,w-24,h-100),Panel,Border);
    DrawTextAt(sx+20,249,L"\u041e\u0431\u0440\u0430\u043d\u0438\u0439 \u0440\u0435\u0433\u0456\u043e\u043d",13,White,true);
    Box(Rect(sx+20,336,w-44,407),RGBColor(24,33,44),Border,12);
    Dot(sx+37,359,RegionColor(selected));
    DrawTextAt(sx+51,348,RegionStatusText(selected),10,RegionColor(selected),true);
    DrawTextAt(sx+35,377,demoMode?UnicodeString(L"\u0414\u0435\u043c\u043e\u043d\u0441\u0442\u0440\u0430\u0446\u0456\u0439\u043d\u0438\u0439 \u0441\u0442\u0430\u0442\u0443\u0441"):(L"\u0414\u0436\u0435\u0440\u0435\u043b\u043e: "+UnicodeString(ApiProviderName(provider))),9,Muted);
    DrawTextAt(sx+20,415,demoMode?L"\u041b\u043e\u043a\u0430\u043b\u044c\u043d\u0430 \u0437\u043c\u0456\u043d\u0430: "+changedAt:L"\u041e\u0441\u0442\u0430\u043d\u043d\u0456\u0439 \u0443\u0441\u043f\u0456\u0445: "+lastSuccess,9,Muted);
    if(!demoMode) {
        DrawTextAt(sx+20,566,L"\u041e\u043d\u043e\u0432\u043b\u0435\u043d\u043d\u044f \u043a\u043e\u0436\u043d\u0456 60 \u0441\u0435\u043a\u0443\u043d\u0434",9,Muted);
        DrawTextAt(sx+20,587,worker?L"\u0417\u0430\u0432\u0430\u043d\u0442\u0430\u0436\u0435\u043d\u043d\u044f\u2026":authPaused?L"\u0414\u043e\u0441\u0442\u0443\u043f \u043f\u0440\u0438\u0437\u0443\u043f\u0438\u043d\u0435\u043d\u043e":apiToken.empty()?L"\u0422\u043e\u043a\u0435\u043d \u0449\u0435 \u043d\u0435 \u0437\u0430\u0434\u0430\u043d\u043e":L"\u0410\u0432\u0442\u043e\u043e\u043d\u043e\u0432\u043b\u0435\u043d\u043d\u044f \u0443\u0432\u0456\u043c\u043a\u043d\u0435\u043d\u043e",9,Muted);
    }
    DrawTextAt(sx+20,610,L"\u041f\u041e\u0417\u041d\u0410\u0427\u0415\u041d\u041d\u042f",8,Muted,true);
    const wchar_t* labels[4]={L"\u0422\u0440\u0438\u0432\u043e\u0433\u0430 \u0432 \u0443\u0441\u0456\u0439 \u043e\u0431\u043b\u0430\u0441\u0442\u0456",L"\u0427\u0430\u0441\u0442\u043a\u043e\u0432\u0430 / \u043f\u043e\u043f\u0435\u0440\u0435\u0434\u043d\u044f \u0437\u0430\u0433\u0440\u043e\u0437\u0430",L"\u041d\u0435\u043c\u0430\u0454 \u0430\u043a\u0442\u0438\u0432\u043d\u043e\u0457 \u0442\u0440\u0438\u0432\u043e\u0433\u0438",L"\u041d\u0435\u043c\u0430\u0454 \u0434\u0430\u043d\u0438\u0445"};
    TColor colors[4]={StateColor(asAlert),RGBColor(244,191,77),StateColor(asClear),StateColor(asUnknown)};
    for(int i=0;i<4;++i) {
        Dot(sx+25,640+i*22,colors[i]);
        DrawTextAt(sx+40,630+i*22,labels[i],9,White);
    }
    Box(Rect(24,h-79,w-24,h-24),RGBColor(47,38,26),RGBColor(89,69,38),12);
    DrawTextAt(42,h-68,demoMode?UnicodeString(L"\u0414\u0415\u041c\u041e\u0420\u0415\u0416\u0418\u041c. \u0412\u0438\u0433\u0430\u0434\u0430\u043d\u0456 \u0434\u0430\u043d\u0456; \u0430\u043a\u0442\u0443\u0430\u043b\u044c\u043d\u0456 \u0442\u0440\u0438\u0432\u043e\u0433\u0438 \u043d\u0435 \u0437\u0430\u0432\u0430\u043d\u0442\u0430\u0436\u0443\u044e\u0442\u044c\u0441\u044f."):connectionText,10,RGBColor(237,196,127),true);
    DrawTextAt(42,h-47,L"\u0414\u043b\u044f \u0440\u0456\u0448\u0435\u043d\u044c \u043a\u043e\u0440\u0438\u0441\u0442\u0443\u0439\u0442\u0435\u0441\u044f \u043e\u0444\u0456\u0446\u0456\u0439\u043d\u0438\u043c\u0438 \u0441\u043f\u043e\u0432\u0456\u0449\u0435\u043d\u043d\u044f\u043c\u0438.  |  C++Builder VCL \u00b7 Win32 \u00b7 x86 ASM",8,Muted);
}

void __fastcall TMainForm::SelectRegion(TObject*) {
    if(regionBox->ItemIndex>=0) { selected=regionBox->ItemIndex; Invalidate(); }
}
void __fastcall TMainForm::ChangeState(TObject*) {
    if(demoMode && model.Cycle(selected)) { changedAt=FormatDateTime(L"hh:nn:ss",Now()); Invalidate(); }
}
void __fastcall TMainForm::ResetDemo(TObject*) {
    if(!demoMode) return;
    model.ResetDemo(); changedAt=FormatDateTime(L"hh:nn:ss",Now()); Invalidate();
}
void __fastcall TMainForm::UnknownData(TObject*) {
    if(!demoMode) return;
    for(int i=0;i<RegionCount;++i) model.states[i]=asUnknown;
    changedAt=FormatDateTime(L"hh:nn:ss",Now()); Invalidate();
}
int TMainForm::HitTest(int x,int y) const {
    // Cities are tested first because they are small enclaves within oblasts.
    for(int i=(int)hitRegions.size()-1;i>=0;--i) if(PtInRegion(hitRegions[i],x,y)) return i;
    return -1;
}
void __fastcall TMainForm::MapMouseDown(TObject*,TMouseButton button,TShiftState,int x,int y) {
    if(button!=mbLeft) return;
    int index=HitTest(x,y);
    if(index>=0) { selected=index; regionBox->ItemIndex=selected; Invalidate(); }
}
void __fastcall TMainForm::MapMouseMove(TObject*,TShiftState,int x,int y) {
    int index=HitTest(x,y);
    Cursor=index>=0?crHandPoint:crDefault;
    UnicodeString hint=index>=0?UnicodeString(Regions[index].name)+L" \u2014 "+RegionStatusText(index):UnicodeString();
    if(Hint!=hint) Hint=hint;
    ShowHint=index>=0;
}
void __fastcall TMainForm::KeyPressed(TObject*,WORD &key,TShiftState) {
    if(demoMode && key=='R' && ActiveControl!=regionBox) { ResetDemo(this); key=0; }
}

UnicodeString TMainForm::RegionStatusText(int region) const {
    if(!demoMode && feed.available) {
        if(feed.snapshot.levels[region]=='P') return L"\u0427\u0430\u0441\u0442\u043a\u043e\u0432\u0430 \u0442\u0440\u0438\u0432\u043e\u0433\u0430";
        if(feed.snapshot.levels[region]=='Y') return L"\u041f\u043e\u043f\u0435\u0440\u0435\u0434\u043d\u044f \u0437\u0430\u0433\u0440\u043e\u0437\u0430";
    }
    return StateText(model.states[region]);
}

void TMainForm::ApplyFeed() {
    for(int i=0;i<RegionCount;++i) model.states[i]=feed.snapshot.states[i];
    Invalidate();
}
void TMainForm::UpdateControls() {
    changeButton->Visible=demoMode;
    resetButton->Visible=demoMode;
    clearButton->Visible=demoMode;
    tokenButton->Visible=!demoMode;
    refreshButton->Visible=!demoMode;
    DWORD elapsed=(DWORD)(GetTickCount()-lastAttemptTick);
    refreshButton->Enabled=!demoMode && !worker && !apiToken.empty() && !authPaused && elapsed>=retryDelay;
    if(worker) refreshButton->Caption=L"\u0417\u0430\u0432\u0430\u043d\u0442\u0430\u0436\u0435\u043d\u043d\u044f\u2026";
    else if(!apiToken.empty() && !authPaused && elapsed<retryDelay)
        refreshButton->Caption=L"\u041e\u043d\u043e\u0432\u043b\u0435\u043d\u043d\u044f \u0447\u0435\u0440\u0435\u0437 "+IntToStr((int)((retryDelay-elapsed+999)/1000))+L" \u0441";
    else refreshButton->Caption=L"\u041e\u043d\u043e\u0432\u0438\u0442\u0438 \u0437\u0430\u0440\u0430\u0437";
}
void __fastcall TMainForm::ModeChanged(TObject*) {
    ++generation;
    demoMode=!liveBox->Checked;
    feed.Unavailable();
    if(demoMode) ResetDemo(this);
    else {
        ApplyFeed();
        connectionText=apiToken.empty()?L"\u0414\u043e\u0434\u0430\u0439\u0442\u0435 API-\u0442\u043e\u043a\u0435\u043d \u2014 \u0434\u0430\u043d\u0456 \u043f\u043e\u043a\u0438 \u043d\u0435\u0434\u043e\u0441\u0442\u0443\u043f\u043d\u0456.":
            authPaused?L"\u0414\u043e\u0441\u0442\u0443\u043f \u043f\u0440\u0438\u0437\u0443\u043f\u0438\u043d\u0435\u043d\u043e. \u041f\u0435\u0440\u0435\u0432\u0456\u0440\u0442\u0435 \u043f\u043e\u0441\u0442\u0430\u0447\u0430\u043b\u044c\u043d\u0438\u043a\u0430 \u043a\u043b\u044e\u0447\u0430 \u0442\u0430 \u0443\u043c\u043e\u0432\u0438 \u0434\u043e\u0441\u0442\u0443\u043f\u0443.":L"\u041e\u0447\u0456\u043a\u0443\u0432\u0430\u043d\u043d\u044f \u043d\u0430\u0441\u0442\u0443\u043f\u043d\u043e\u0433\u043e \u0437\u0430\u043f\u0438\u0442\u0443 \u0434\u043e API\u2026";
    }
    UpdateControls(); Invalidate();
}
void __fastcall TMainForm::EditToken(TObject*) {
    TForm* dialog=new TForm(this,0);
    try {
        dialog->Caption=L"\u041f\u0456\u0434\u043a\u043b\u044e\u0447\u0435\u043d\u043d\u044f API";
        dialog->BorderStyle=bsDialog;
        dialog->Position=poOwnerFormCenter;
        dialog->ClientWidth=480; dialog->ClientHeight=310;
        TLabel* label=new TLabel(dialog); label->Parent=dialog;
        label->SetBounds(20,16,440,44); label->AutoSize=false; label->WordWrap=true;
        label->Caption=L"\u041e\u0431\u0435\u0440\u0456\u0442\u044c \u0441\u0435\u0440\u0432\u0456\u0441, \u044f\u043a\u0438\u0439 \u0432\u0438\u0434\u0430\u0432 \u0432\u0430\u0448 \u043a\u043b\u044e\u0447. \u041a\u043b\u044e\u0447\u0456 \u0440\u0456\u0437\u043d\u0438\u0445 \u0441\u0435\u0440\u0432\u0456\u0441\u0456\u0432 \u043d\u0435 \u0432\u0437\u0430\u0454\u043c\u043e\u0437\u0430\u043c\u0456\u043d\u043d\u0456.";
        TComboBox* sourceBox=new TComboBox(dialog); sourceBox->Parent=dialog;
        sourceBox->SetBounds(20,68,440,28); sourceBox->Style=csDropDownList;
        sourceBox->Items->Add(L"alerts.in.ua"); sourceBox->Items->Add(L"UkraineAlarm / Stfalcon");
        sourceBox->ItemIndex=(int)provider;
        TEdit* edit=new TEdit(dialog); edit->Parent=dialog;
        edit->SetBounds(20,116,440,26); edit->PasswordChar=L'*'; edit->MaxLength=2048;
        edit->Text=apiToken.c_str();
        TCheckBox* remember=new TCheckBox(dialog); remember->Parent=dialog;
        remember->SetBounds(20,154,440,24); remember->Caption=L"\u0417\u0431\u0435\u0440\u0456\u0433\u0430\u0442\u0438 \u043a\u043b\u044e\u0447 \u043d\u0430 \u0446\u044c\u043e\u043c\u0443 \u043a\u043e\u043c\u043f\u2019\u044e\u0442\u0435\u0440\u0456";
        remember->Checked=rememberKey;
        TLabel* help=new TLabel(dialog); help->Parent=dialog;
        help->SetBounds(20,190,440,48); help->AutoSize=false; help->WordWrap=true;
        help->Caption=L"\u041a\u043b\u044e\u0447 \u0437\u0431\u0435\u0440\u0456\u0433\u0430\u0454\u0442\u044c\u0441\u044f \u0443 \u0437\u0430\u0445\u0438\u0449\u0435\u043d\u043e\u043c\u0443 \u0441\u0445\u043e\u0432\u0438\u0449\u0456 Windows. \u0417\u043d\u0456\u043c\u0456\u0442\u044c \u043f\u0440\u0430\u043f\u043e\u0440\u0435\u0446\u044c \u0456 \u0437\u0430\u0441\u0442\u043e\u0441\u0443\u0439\u0442\u0435 \u043d\u0430\u043b\u0430\u0448\u0442\u0443\u0432\u0430\u043d\u043d\u044f, \u0449\u043e\u0431 \u0437\u0430\u043b\u0438\u0448\u0438\u0442\u0438 \u0439\u043e\u0433\u043e \u043b\u0438\u0448\u0435 \u043d\u0430 \u0446\u0435\u0439 \u0441\u0435\u0430\u043d\u0441.";
        TButton* ok=new TButton(dialog); ok->Parent=dialog;
        ok->SetBounds(230,260,110,32); ok->Caption=L"\u0417\u0430\u0441\u0442\u043e\u0441\u0443\u0432\u0430\u0442\u0438"; ok->ModalResult=mrOk; ok->Default=true;
        TButton* cancel=new TButton(dialog); cancel->Parent=dialog;
        cancel->SetBounds(350,260,110,32); cancel->Caption=L"\u0421\u043a\u0430\u0441\u0443\u0432\u0430\u0442\u0438"; cancel->ModalResult=mrCancel; cancel->Cancel=true;
        dialog->ActiveControl=edit;
        if(dialog->ShowModal()==mrOk) {
            std::wstring next=edit->Text.Trim().c_str();
            if(!IsValidApiToken(next)) {
                Application->MessageBox(L"\u0412\u0432\u0435\u0434\u0456\u0442\u044c \u0442\u043e\u043a\u0435\u043d \u0431\u0435\u0437 \u043f\u0440\u043e\u0431\u0456\u043b\u0456\u0432 \u0456 \u043f\u0435\u0440\u0435\u043d\u043e\u0441\u0456\u0432 \u0440\u044f\u0434\u043a\u0430.",L"\u041d\u0435\u043a\u043e\u0440\u0435\u043a\u0442\u043d\u0438\u0439 \u0442\u043e\u043a\u0435\u043d",MB_OK|MB_ICONINFORMATION);
                return;
            }
            ApiProvider nextProvider=(ApiProvider)sourceBox->ItemIndex;
            bool stored=remember->Checked?SaveApiCredential(ApiCredentialTarget,next,nextProvider):DeleteApiCredential(ApiCredentialTarget);
            if(!stored) {
                Application->MessageBox(L"\u041d\u0435 \u0432\u0434\u0430\u043b\u043e\u0441\u044f \u0437\u043c\u0456\u043d\u0438\u0442\u0438 \u0437\u0431\u0435\u0440\u0435\u0436\u0435\u043d\u0438\u0439 \u043a\u043b\u044e\u0447 \u0443 Windows. \u041d\u0430\u043b\u0430\u0448\u0442\u0443\u0432\u0430\u043d\u043d\u044f \u043d\u0435 \u0437\u0430\u0441\u0442\u043e\u0441\u043e\u0432\u0430\u043d\u043e.",L"\u0417\u0431\u0435\u0440\u0435\u0436\u0435\u043d\u043d\u044f \u043a\u043b\u044e\u0447\u0430",MB_OK|MB_ICONERROR);
                return;
            }
            rememberKey=remember->Checked;
            apiToken=next; provider=nextProvider; regionDirectory.clear();
            ++generation; authPaused=false; feed.Unavailable(); ApplyFeed();
            connectionText=L"\u0422\u043e\u043a\u0435\u043d \u0437\u0430\u0434\u0430\u043d\u043e. \u041e\u0447\u0456\u043a\u0443\u0432\u0430\u043d\u043d\u044f \u0437\u0430\u043f\u0438\u0442\u0443 \u0434\u043e API\u2026";
            UpdateControls();
        }
    } __finally { delete dialog; }
}
void TMainForm::StartRequest() {
    if(demoMode || worker || apiToken.empty() || authPaused || (DWORD)(GetTickCount()-lastAttemptTick)<retryDelay) return;
    lastAttemptTick=GetTickCount();
    retryDelay=60000UL;
    try {
        worker=new TAlertWorker(apiToken,generation,provider,regionDirectory);
        worker->Start();
        connectionText=(L"\u0417\u0430\u0432\u0430\u043d\u0442\u0430\u0436\u0435\u043d\u043d\u044f \u0437 "+UnicodeString(ApiProviderName(provider))+L"\u2026");
    } catch(...) {
        delete worker; worker=0; feed.Unavailable(); ApplyFeed();
        connectionText=L"\u041d\u0435 \u0432\u0434\u0430\u043b\u043e\u0441\u044f \u0437\u0430\u043f\u0443\u0441\u0442\u0438\u0442\u0438 \u0437\u0430\u043f\u0438\u0442. \u041f\u043e\u0432\u0442\u043e\u0440 \u0447\u0435\u0440\u0435\u0437 \u0445\u0432\u0438\u043b\u0438\u043d\u0443.";
    }
    UpdateControls(); Invalidate();
}
void __fastcall TMainForm::RefreshFeed(TObject*) { StartRequest(); }
void __fastcall TMainForm::FeedTick(TObject*) {
    if(worker && WaitForSingleObject((HANDLE)worker->Handle,0)==WAIT_OBJECT_0) {
        ApiResult result=worker->result;
        bool current=worker->generation==generation && !demoMode;
        delete worker; worker=0;
        lastAttemptTick=GetTickCount();
        // Respect server throttling even if the user changed mode during the request.
        retryDelay=result.retrySeconds*1000UL;
        if(current) {
            if(!result.regionDirectory.empty()) regionDirectory=result.regionDirectory;
            if(result.error==aeNone) {
                feed.Accept(result.snapshot,GetTickCount());
                lastSuccess=FormatDateTime(L"dd.mm hh:nn:ss",Now());
                connectionText=(L"\u041e\u0442\u0440\u0438\u043c\u0430\u043d\u043e \u0434\u0430\u043d\u0456 "+UnicodeString(ApiProviderName(provider))+L". \u0427\u0430\u0441 \u0432\u0456\u0434\u043f\u043e\u0432\u0456\u0434\u0456: ")+lastSuccess+L". \u0410\u0432\u0442\u043e\u043e\u043d\u043e\u0432\u043b\u0435\u043d\u043d\u044f: 60 \u0441.";
            } else {
                feed.Unavailable();
                if(result.error==aeFormat) regionDirectory.clear();
                if(result.error==aePreparing) connectionText=L"\u0414\u043e\u0432\u0456\u0434\u043d\u0438\u043a \u0440\u0435\u0433\u0456\u043e\u043d\u0456\u0432 \u043e\u0442\u0440\u0438\u043c\u0430\u043d\u043e. \u0421\u0442\u0430\u0442\u0443\u0441\u0438 \u0437\u0430\u0432\u0430\u043d\u0442\u0430\u0436\u0430\u0442\u044c\u0441\u044f \u0447\u0435\u0440\u0435\u0437 \u0445\u0432\u0438\u043b\u0438\u043d\u0443 \u0437 \u0443\u0440\u0430\u0445\u0443\u0432\u0430\u043d\u043d\u044f\u043c \u043b\u0456\u043c\u0456\u0442\u0443 API.";
                else if(result.error==aeToken || result.error==aeForbidden) {
                    authPaused=true;
                    if(result.error==aeForbidden)
                        connectionText=L"HTTP 403: API \u0437\u0430\u0431\u043e\u0440\u043e\u043d\u0438\u0432 \u0434\u043e\u0441\u0442\u0443\u043f. \u041f\u0435\u0440\u0435\u0432\u0456\u0440\u0442\u0435 \u043f\u0440\u0430\u0432\u0430 \u043a\u043b\u044e\u0447\u0430 \u0430\u0431\u043e \u043e\u0431\u043c\u0435\u0436\u0435\u043d\u043d\u044f IP \u0443 \u043f\u043e\u0441\u0442\u0430\u0447\u0430\u043b\u044c\u043d\u0438\u043a\u0430.";
                    else if(result.status==401)
                        connectionText=(L"HTTP 401: "+UnicodeString(ApiProviderName(provider))+L" \u043d\u0435 \u043f\u0440\u0438\u0439\u043d\u044f\u0432 \u043a\u043b\u044e\u0447. \u041f\u0435\u0440\u0435\u0432\u0456\u0440\u0442\u0435 \u0432\u0438\u0431\u0440\u0430\u043d\u0438\u0439 \u0441\u0435\u0440\u0432\u0456\u0441 \u0442\u0430 \u043a\u043b\u044e\u0447.");
                    else
                        connectionText=L"\u041d\u0435\u043a\u043e\u0440\u0435\u043a\u0442\u043d\u0438\u0439 \u0444\u043e\u0440\u043c\u0430\u0442 \u043a\u043b\u044e\u0447\u0430. \u0412\u0432\u0435\u0434\u0456\u0442\u044c \u043a\u043b\u044e\u0447 \u043e\u0431\u0440\u0430\u043d\u043e\u0433\u043e \u0441\u0435\u0440\u0432\u0456\u0441\u0443.";
                } else if(result.status==429) connectionText=L"\u041b\u0456\u043c\u0456\u0442 API. \u041f\u043e\u0432\u0442\u043e\u0440 \u0447\u0435\u0440\u0435\u0437 "+IntToStr((int)result.retrySeconds)+L" \u0441; \u043f\u043e\u0442\u043e\u0447\u043d\u0456 \u0434\u0430\u043d\u0456 \u043d\u0435\u0434\u043e\u0441\u0442\u0443\u043f\u043d\u0456.";
                else if(result.error==aeFormat) connectionText=L"\u041d\u0435\u043a\u043e\u0440\u0435\u043a\u0442\u043d\u0430 \u0432\u0456\u0434\u043f\u043e\u0432\u0456\u0434\u044c API. \u0414\u0430\u043d\u0456 \u043d\u0435 \u0437\u0430\u0441\u0442\u043e\u0441\u043e\u0432\u0430\u043d\u043e; \u043f\u043e\u0432\u0442\u043e\u0440 \u0447\u0435\u0440\u0435\u0437 \u0445\u0432\u0438\u043b\u0438\u043d\u0443.";
                else if(result.error==aeStale) connectionText=L"\u0412\u0456\u0434\u043f\u043e\u0432\u0456\u0434\u044c API \u0437\u0430\u0441\u0442\u0430\u0440\u0456\u043b\u0430. \u041f\u043e\u0442\u043e\u0447\u043d\u0456 \u0434\u0430\u043d\u0456 \u043d\u0435\u0434\u043e\u0441\u0442\u0443\u043f\u043d\u0456.";
                else if(result.status) connectionText=L"\u041f\u043e\u043c\u0438\u043b\u043a\u0430 API: HTTP "+IntToStr((int)result.status)+L". \u041f\u043e\u0442\u043e\u0447\u043d\u0456 \u0434\u0430\u043d\u0456 \u043d\u0435\u0434\u043e\u0441\u0442\u0443\u043f\u043d\u0456.";
                else connectionText=L"\u041d\u0435\u043c\u0430\u0454 \u0437\u0432\u2019\u044f\u0437\u043a\u0443 \u0437 API (\u043a\u043e\u0434 "+IntToStr((int)result.networkError)+L"). \u041f\u043e\u0432\u0442\u043e\u0440 \u0447\u0435\u0440\u0435\u0437 \u0445\u0432\u0438\u043b\u0438\u043d\u0443.";
            }
            ApplyFeed();
        }
        Invalidate();
    }
    if(!demoMode) {
        if(feed.Expire(GetTickCount())) {
            connectionText=L"\u041f\u043e\u043d\u0430\u0434 90 \u0441\u0435\u043a\u0443\u043d\u0434 \u0431\u0435\u0437 \u043f\u0456\u0434\u0442\u0432\u0435\u0440\u0434\u0436\u0435\u043d\u043d\u044f API. \u041f\u043e\u0442\u043e\u0447\u043d\u0456 \u0434\u0430\u043d\u0456 \u043d\u0435\u0434\u043e\u0441\u0442\u0443\u043f\u043d\u0456.";
            ApplyFeed();
        }
        StartRequest();
    }
    UpdateControls();
}
