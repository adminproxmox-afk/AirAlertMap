#ifndef WEB_MAP_FORM_H
#define WEB_MAP_FORM_H
#include <Vcl.Forms.hpp>
#include <Vcl.StdCtrls.hpp>
#include <Vcl.Edge.hpp>

class TWebMapForm : public TForm {
    Vcl::Edge::TEdgeBrowser* browser;
    TLabel* notice;
    UnicodeString localUri;
    UnicodeString snapshot;
    bool ready;
    void __fastcall Created(Vcl::Edge::TCustomEdgeBrowser* Sender,HRESULT result);
    void __fastcall Navigated(Vcl::Edge::TCustomEdgeBrowser* Sender,bool success,Winapi::Webview2::COREWEBVIEW2_WEB_ERROR_STATUS status);
    void __fastcall Navigating(Vcl::Edge::TCustomEdgeBrowser* Sender,Vcl::Edge::TNavigationStartingEventArgs* args);
    void __fastcall NewWindow(Vcl::Edge::TCustomEdgeBrowser* Sender,Vcl::Edge::TNewWindowRequestedEventArgs* args);
    void __fastcall Message(Vcl::Edge::TCustomEdgeBrowser* Sender,Vcl::Edge::TWebMessageReceivedEventArgs* args);
public:
    int SelectedRegion;
    TNotifyEvent OnRegionSelected;
    __fastcall TWebMapForm(TComponent* owner,const UnicodeString& page,const UnicodeString& profile);
    void UpdateSnapshot(const UnicodeString& json);
};
#endif
