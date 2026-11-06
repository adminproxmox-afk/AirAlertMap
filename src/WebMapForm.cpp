#include <vcl.h>
#pragma hdrstop
#include "WebMapForm.h"
#include <shlwapi.h>
#pragma link "shlwapi.lib"
#pragma link "vcledge.lib"
#pragma package(smart_init)

__fastcall TWebMapForm::TWebMapForm(TComponent* owner,const UnicodeString& page,const UnicodeString& profile)
 : TForm(owner),browser(0),notice(0),ready(false),OnCommand(0) {
    Caption=L"AirAlertMap | \u0406\u043d\u0442\u0435\u0440\u0430\u043a\u0442\u0438\u0432\u043d\u0430 \u043c\u0430\u043f\u0430";
    Width=1280; Height=850; Position=poOwnerFormCenter;
    Constraints->MinWidth=820; Constraints->MinHeight=620;
    Color=(TColor)0x201810;
    notice=new TLabel(this); notice->Parent=this; notice->Align=alClient;
    notice->WordWrap=true; notice->Font->Name=L"Segoe UI"; notice->Font->Size=13;
    notice->Font->Color=clWhite; notice->Caption=L"\u0417\u0430\u0432\u0430\u043d\u0442\u0430\u0436\u0435\u043d\u043d\u044f \u0456\u043d\u0442\u0435\u0440\u0430\u043a\u0442\u0438\u0432\u043d\u043e\u0457 \u043c\u0430\u043f\u0438\u2026";
    wchar_t uri[8192]; DWORD length=8192;
    if(FAILED(UrlCreateFromPathW(page.c_str(),uri,&length,0))) {
        notice->Caption=L"\u041d\u0435 \u0432\u0434\u0430\u043b\u043e\u0441\u044f \u0432\u0456\u0434\u043a\u0440\u0438\u0442\u0438 \u043b\u043e\u043a\u0430\u043b\u044c\u043d\u0443 \u0441\u0442\u043e\u0440\u0456\u043d\u043a\u0443 \u043c\u0430\u043f\u0438."; return;
    }
    localUri=uri;
    browser=new Vcl::Edge::TEdgeBrowser(this); browser->Parent=this; browser->Align=alClient;
    browser->UserDataFolder=profile;
    browser->OnCreateWebViewCompleted=Created;
    browser->OnNavigationStarting=Navigating;
    browser->OnNavigationCompleted=Navigated;
    browser->OnWebMessageReceived=Message;
    browser->OnNewWindowRequested=NewWindow;
    browser->CreateWebView();
}
void __fastcall TWebMapForm::Created(Vcl::Edge::TCustomEdgeBrowser*,HRESULT result) {
    if(FAILED(result)) {
        browser->Visible=false;
        notice->Caption=L"\u0414\u043b\u044f \u0456\u043d\u0442\u0435\u0440\u0444\u0435\u0439\u0441\u0443 \u043f\u043e\u0442\u0440\u0456\u0431\u0435\u043d Microsoft Edge WebView2 Runtime \u0442\u0430 WebView2Loader.dll \u043f\u043e\u0440\u0443\u0447 \u0456\u0437 \u043f\u0440\u043e\u0433\u0440\u0430\u043c\u043e\u044e. \u0412\u0441\u0442\u0430\u043d\u043e\u0432\u0456\u0442\u044c Runtime \u0430\u0431\u043e \u043f\u043e\u0432\u0442\u043e\u0440\u0456\u0442\u044c \u0437\u0431\u0456\u0440\u043a\u0443.";
        return;
    }
    browser->DefaultContextMenusEnabled=false;
    browser->StatusBarEnabled=false;
    browser->Navigate(localUri);
}
void __fastcall TWebMapForm::Navigating(Vcl::Edge::TCustomEdgeBrowser*,Vcl::Edge::TNavigationStartingEventArgs* args) {
    wchar_t* uri=NULL;
    if(SUCCEEDED(args->ArgsInterface->Get_uri(uri))) {
        bool allowed=UnicodeString(uri)==localUri;
        CoTaskMemFree(uri);
        if(!allowed) args->ArgsInterface->Set_Cancel(TRUE);
    } else args->ArgsInterface->Set_Cancel(TRUE);
    ready=false;
}
void __fastcall TWebMapForm::NewWindow(Vcl::Edge::TCustomEdgeBrowser*,Vcl::Edge::TNewWindowRequestedEventArgs* args) {
    args->ArgsInterface->Set_Handled(TRUE);
}
void __fastcall TWebMapForm::Navigated(Vcl::Edge::TCustomEdgeBrowser*,bool success,Winapi::Webview2::COREWEBVIEW2_WEB_ERROR_STATUS) {
    ready=success && browser->LocationURL==localUri;
    notice->Visible=!ready;
    // The page's ready message can precede NavigationCompleted. Request a full
    // replay after the trusted document is ready, including territory metadata.
    if(ready && OnCommand) {
        snapshot=L""; // A new document needs a replay even if its JSON is identical.
        OnCommand(this,L"{\"version\":1,\"type\":\"ready\"}");
    }
    else if(ready && !snapshot.IsEmpty()) browser->DefaultInterface->PostWebMessageAsJson(const_cast<wchar_t*>(snapshot.c_str()));
}
void __fastcall TWebMapForm::Message(Vcl::Edge::TCustomEdgeBrowser*,Vcl::Edge::TWebMessageReceivedEventArgs* args) {
    wchar_t* source=NULL; wchar_t* message=NULL;
    if(FAILED(args->ArgsInterface->Get_Source(source))) return;
    bool allowed=ready && UnicodeString(source)==localUri; CoTaskMemFree(source);
    if(!allowed || FAILED(args->ArgsInterface->TryGetWebMessageAsString(message))) return;
    UnicodeString text(message); CoTaskMemFree(message);
    if(text.Length()>8192) return;
    if(OnCommand) OnCommand(this,text);
}
void TWebMapForm::UpdateSnapshot(const UnicodeString& json) {
    if(snapshot==json) return;
    snapshot=json;
    if(ready && browser->LocationURL==localUri) browser->DefaultInterface->PostWebMessageAsJson(const_cast<wchar_t*>(snapshot.c_str()));
}
