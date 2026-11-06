#include "UiCommand.h"
#include "Preferences.h"
#include <System.JSON.hpp>
#include <System.SysUtils.hpp>
#include <memory>
#include <set>
using namespace System;
using namespace System::Json;
static bool Text(TJSONObject* root,const wchar_t* name,UnicodeString& out) {
    TJSONString* value=dynamic_cast<TJSONString*>(root->GetValue(UnicodeString(name)));
    if(!value) return false; out=value->Value(); return true;
}
static bool Number(TJSONObject* root,const wchar_t* name,int& out) {
    TJSONNumber* value=dynamic_cast<TJSONNumber*>(root->GetValue(UnicodeString(name)));
    if(!value) return false;
    UnicodeString text=value->Value(); out=System::Sysutils::StrToIntDef(text,-999);
    return System::Sysutils::IntToStr(out)==text;
}
static bool ReadBoolean(TJSONObject* root,const wchar_t* name,bool& out) {
    TJSONBool* value=dynamic_cast<TJSONBool*>(root->GetValue(UnicodeString(name)));
    if(!value) return false; out=value->AsBoolean; return true;
}
bool ParseUiCommand(const UnicodeString& json,UiCommand& command) {
    if(json.IsEmpty() || json.Length()>8192) return false;
    try {
        std::auto_ptr<TJSONValue> value(TJSONObject::ParseJSONValue(json,false,true));
        TJSONObject* root=dynamic_cast<TJSONObject*>(value.get());
        if(!root) return false;
        std::set<std::wstring> fields;
        for(int i=0;i<root->Count;++i) if(!fields.insert(root->Pairs[i]->JsonString->Value().c_str()).second) return false;
        int version=0; UnicodeString type;
        if(!Number(root,L"version",version) || version!=1 || !Text(root,L"type",type)) return false;
        UiCommand next;
        if(type==L"ready") next.type=ucReady;
        else if(type==L"storageStats") next.type=ucStorageStats;
        else if(type==L"archiveHistory") next.type=ucArchiveHistory;
        else if(type==L"copyDiagnostics") next.type=ucCopyDiagnostics;
        else if(type==L"retention") {
            bool confirmed=false;
            if(!Number(root,L"days",next.days)||(next.days!=0&&next.days!=90&&next.days!=180&&next.days!=365)||!ReadBoolean(root,L"confirm",confirmed)||!confirmed)return false;
            next.type=ucRetention;
        }
        else if(type==L"refresh") next.type=ucRefresh;
        else if(type==L"select") {
            if(!Number(root,L"region",next.region) || next.region<0 || next.region>=RegionCount) return false;
            next.type=ucSelect;
        } else if(type==L"mode") {
            if(!ReadBoolean(root,L"live",next.live)) return false; next.type=ucMode;
        } else if(type==L"saveSettings") {
            UnicodeString source,token;
            if(!Text(root,L"provider",source) || !Text(root,L"token",token) || !ReadBoolean(root,L"remember",next.remember)) return false;
            if(source==L"ukrainealarm") next.provider=apUkraineAlarm;
            else if(source==L"alerts.in.ua") next.provider=apAlertsInUa;
            else return false;
            UnicodeString trimmed=token.Trim();
            next.token=std::wstring(trimmed.c_str(),trimmed.Length());
            if(!next.token.empty() && !IsValidApiToken(next.token)) return false;
            next.type=ucSaveSettings;
        } else if(type==L"forgetKey") {
            bool confirm=false; if(!ReadBoolean(root,L"confirm",confirm) || !confirm) return false;
            next.type=ucForgetKey;
        } else if(type==L"demoReset") next.type=ucDemoReset;
        else if(type==L"demoCycle") next.type=ucDemoCycle;
        else if(type==L"demoUnknown") next.type=ucDemoUnknown;
        else if(type==L"history") {
            if(!Number(root,L"days",next.days) || (next.days!=0 && next.days!=1 && next.days!=7 && next.days!=30)) return false;
            if(root->GetValue(UnicodeString(L"territory"))) {
                UnicodeString id;if(!Text(root,L"territory",id)||id.Length()>10)return false;
                for(int i=1;i<=id.Length();++i)if(id[i]<L'0'||id[i]>L'9')return false;
                next.territory=UTF8String(id).c_str();
            }
            next.type=ucHistory;
        } else if(type==L"preferences") {
            TJSONObject* preferences=dynamic_cast<TJSONObject*>(root->GetValue(UnicodeString(L"preferences")));
            if(!preferences)return false;
            UTF8String encoded(preferences->ToJSON());
            if(!NormalizePreferences(std::string(encoded.c_str(),encoded.Length()),next.preferences))return false;
            next.type=ucPreferences;
        } else if(type==L"export") {
            UnicodeString format; if(!Text(root,L"format",format) || (format!=L"csv" && format!=L"json" && format!=L"html")) return false;
            next.format=System::UTF8String(format).c_str(); next.type=ucExport;
        } else return false;
        command=next; return true;
    } catch(...) { return false; }
}

