#include <vcl.h>
#pragma hdrstop
#include "MainForm.h"
#include "MapData.h"
#include <math.h>
#pragma package(smart_init)
#pragma resource "MainForm.dfm"

TMainForm* MainForm;
static TColor RGBColor(int r, int g, int b) { return (TColor)RGB(r,g,b); }
static const TColor Background = (TColor)0x17110D;
static const TColor Panel = (TColor)0x231B15;
static const TColor Border = (TColor)0x3B3026;
static const TColor White = (TColor)0xF4F0EB;
static const TColor Muted = (TColor)0xAA9B8D;
static const TColor Accent = (TColor)0xDCD557;

__fastcall TMainForm::TMainForm(TComponent* Owner) : TForm(Owner), regionBox(0), changeButton(0), resetButton(0), clearButton(0), selected(7) {
    Caption = L"\u041c\u0430\u043f\u0430 \u0442\u0440\u0438\u0432\u043e\u0433 \u0423\u043a\u0440\u0430\u0457\u043d\u0438 \u2014 DEMO | C++Builder + x86 ASM";
    Color = Background;
    Font->Name = L"Segoe UI";
    Font->Size = 10;
    Font->Color = White;
    Position = poScreenCenter;
    ClientWidth = 1280;
    ClientHeight = 850;
    Constraints->MinWidth = 1120;
    Constraints->MinHeight = 820;
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
    ResetDemo(this);
    ResizeView(this);
}

__fastcall TMainForm::~TMainForm() { ClearRegions(); }
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
UnicodeString TMainForm::StateText(int state) const {
    if(state==asAlert) return L"\u041f\u043e\u0432\u0456\u0442\u0440\u044f\u043d\u0430 \u0442\u0440\u0438\u0432\u043e\u0433\u0430";
    if(state==asClear) return L"\u0411\u0435\u0437 \u0442\u0440\u0438\u0432\u043e\u0433\u0438 (\u0434\u0435\u043c\u043e)";
    return L"\u041d\u0435\u043c\u0430\u0454 \u0434\u0430\u043d\u0438\u0445";
}

void __fastcall TMainForm::ResizeView(TObject*) {
    if (!regionBox) return;
    const int sx=ClientWidth-304;
    regionBox->SetBounds(sx+20,286,244,30);
    changeButton->SetBounds(sx+20,440,244,36);
    resetButton->SetBounds(sx+20,490,244,34);
    clearButton->SetBounds(sx+20,534,244,34);
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
    Box(Rect(w-245,30,w-24,66),RGBColor(62,49,30),RGBColor(108,83,42),16);
    Dot(w-225,48,RGBColor(240,190,104));
    DrawTextAt(w-211,37,L"\u0414\u0415\u041c\u041e \u00b7 \u0412\u0418\u0413\u0410\u0414\u0410\u041d\u0406 \u0414\u0410\u041d\u0406",9,RGBColor(240,190,104),true);
    Canvas->Pen->Color=Border; Canvas->MoveTo(24,101); Canvas->LineTo(w-24,101);

    const int cardWidth=(w-72)/3;
    const int states[3]={asAlert,asClear,asUnknown};
    const wchar_t* captions[3]={L"\u0420\u0435\u0433\u0456\u043e\u043d\u0456\u0432 \u0456\u0437 \u0442\u0440\u0438\u0432\u043e\u0433\u043e\u044e",L"\u0420\u0435\u0433\u0456\u043e\u043d\u0456\u0432 \u0431\u0435\u0437 \u0442\u0440\u0438\u0432\u043e\u0433\u0438",L"\u041d\u0435\u043c\u0430\u0454 \u0434\u0430\u043d\u0438\u0445"};
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
        TColor fill=model.states[i]==asAlert?RGBColor(124,50,62):
                    model.states[i]==asClear?RGBColor(32,74,73):RGBColor(49,59,74);
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
    const int state=model.states[selected];
    Box(Rect(sx+20,336,w-44,407),RGBColor(24,33,44),Border,12);
    Dot(sx+37,359,StateColor(state));
    DrawTextAt(sx+51,348,StateText(state),10,StateColor(state),true);
    DrawTextAt(sx+35,377,L"\u0414\u0435\u043c\u043e\u043d\u0441\u0442\u0440\u0430\u0446\u0456\u0439\u043d\u0438\u0439 \u0441\u0442\u0430\u0442\u0443\u0441",9,Muted);
    DrawTextAt(sx+20,415,L"\u041b\u043e\u043a\u0430\u043b\u044c\u043d\u0430 \u0437\u043c\u0456\u043d\u0430: "+changedAt,9,Muted);
    DrawTextAt(sx+20,592,L"\u041f\u041e\u0417\u041d\u0410\u0427\u0415\u041d\u041d\u042f",8,Muted,true);
    const wchar_t* labels[3]={L"\u0422\u0440\u0438\u0432\u043e\u0433\u0430 (\u0434\u0435\u043c\u043e)",L"\u0411\u0435\u0437 \u0442\u0440\u0438\u0432\u043e\u0433\u0438 (\u0434\u0435\u043c\u043e)",L"\u041d\u0435\u043c\u0430\u0454 \u0434\u0430\u043d\u0438\u0445"};
    for(int i=0;i<3;++i) {
        Dot(sx+25,625+i*27,StateColor(states[i]));
        DrawTextAt(sx+40,615+i*27,labels[i],9,White);
    }
    Box(Rect(24,h-79,w-24,h-24),RGBColor(47,38,26),RGBColor(89,69,38),12);
    DrawTextAt(42,h-68,L"\u0414\u0415\u041c\u041e\u0420\u0415\u0416\u0418\u041c. \u041f\u043e\u0442\u043e\u0447\u043d\u0456 \u0442\u0440\u0438\u0432\u043e\u0433\u0438 \u043d\u0435 \u0437\u0430\u0432\u0430\u043d\u0442\u0430\u0436\u0443\u044e\u0442\u044c\u0441\u044f. \u0414\u043b\u044f \u0440\u0456\u0448\u0435\u043d\u044c \u043a\u043e\u0440\u0438\u0441\u0442\u0443\u0439\u0442\u0435\u0441\u044f \u043e\u0444\u0456\u0446\u0456\u0439\u043d\u0438\u043c\u0438 \u0441\u043f\u043e\u0432\u0456\u0449\u0435\u043d\u043d\u044f\u043c\u0438.",10,RGBColor(237,196,127),true);
    DrawTextAt(42,h-47,L"C++Builder VCL \u00b7 Win32 \u00b7 \u043f\u0456\u0434\u0440\u0430\u0445\u0443\u043d\u043e\u043a \u0430\u043a\u0442\u0438\u0432\u043d\u0438\u0445 \u0440\u0435\u0433\u0456\u043e\u043d\u0456\u0432: x86 ASM  |  Tab: \u0435\u043b\u0435\u043c\u0435\u043d\u0442\u0438 \u043a\u0435\u0440\u0443\u0432\u0430\u043d\u043d\u044f \u00b7 R: \u0441\u043a\u0438\u043d\u0443\u0442\u0438 \u0434\u0435\u043c\u043e",8,Muted);
}

void __fastcall TMainForm::SelectRegion(TObject*) {
    if(regionBox->ItemIndex>=0) { selected=regionBox->ItemIndex; Invalidate(); }
}
void __fastcall TMainForm::ChangeState(TObject*) {
    if(model.Cycle(selected)) { changedAt=FormatDateTime(L"hh:nn:ss",Now()); Invalidate(); }
}
void __fastcall TMainForm::ResetDemo(TObject*) {
    model.ResetDemo(); changedAt=FormatDateTime(L"hh:nn:ss",Now()); Invalidate();
}
void __fastcall TMainForm::UnknownData(TObject*) {
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
    UnicodeString hint=index>=0?UnicodeString(Regions[index].name)+L" \u2014 "+StateText(model.states[index]):UnicodeString();
    if(Hint!=hint) Hint=hint;
    ShowHint=index>=0;
}
void __fastcall TMainForm::KeyPressed(TObject*,WORD &key,TShiftState) {
    if(key=='R' && ActiveControl!=regionBox) { ResetDemo(this); key=0; }
}
