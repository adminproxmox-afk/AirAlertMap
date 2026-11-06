#include "Preferences.h"
#include <System.JSON.hpp>
#include <System.SysUtils.hpp>
#include <memory>
#include <set>
#include <math.h>
using namespace System;
using namespace System::Json;
std::string DefaultPreferences() {
 return "{\"version\":1,\"favorites\":[],\"home\":\"\",\"selectedTerritory\":\"\",\"view\":{\"x\":0,\"y\":0,\"w\":1000,\"h\":700},\"detailLevel\":\"auto\",\"showLabels\":true,\"showBorders\":true,\"showStatuses\":true,\"theme\":\"muted\",\"brightness\":100,\"borderWidth\":100}";
}
bool IsTerritoryKey(const std::string& key) {
 unsigned p=key.find(':');
 if(key.size()>32 || p==std::string::npos || (key.substr(0,p)!="State" && key.substr(0,p)!="District" && key.substr(0,p)!="Community")) return false;
 std::string code=key.substr(p+1);
 if(code.size()<4 || code.size()>11 || code.substr(0,2)!="UA") return false;
 for(unsigned i=2;i<code.size();++i) if(code[i]<'0' || code[i]>'9') return false;
 return true;
}
static bool Keys(TJSONObject* object,const wchar_t* const* allowed,int count) {
 if(!object || object->Count!=count) return false;
 std::set<std::wstring> seen;
 for(int i=0;i<object->Count;++i) {
  std::wstring name=object->Pairs[i]->JsonString->Value().c_str(); bool found=false;
  if(!seen.insert(name).second) return false;
  for(int j=0;j<count;++j) if(name==allowed[j]) found=true;
  if(!found) return false;
 }
 return true;
}
static bool ReadText(TJSONObject* o,const wchar_t* key,std::string& out) {
 TJSONString* v=dynamic_cast<TJSONString*>(o->GetValue(UnicodeString(key)));
 if(!v || v->Value().Length()>64) return false;
 UTF8String text(v->Value()); out=std::string(text.c_str(),text.Length());return true;
}
static bool ReadNumber(TJSONObject* o,const wchar_t* key,double& out) {
 TJSONNumber* v=dynamic_cast<TJSONNumber*>(o->GetValue(UnicodeString(key)));
 if(!v)return false;out=v->AsDouble;return out==out && fabs(out)<1e10;
}
bool NormalizePreferences(const std::string& json,std::string& normalized) {
 if(json.empty() || json.size()>8192 || json.find('\0')!=std::string::npos) return false;
 try {
  std::auto_ptr<TJSONValue> root(TJSONObject::ParseJSONValue(UTF8String(json.c_str(),json.size()),false,true));
  TJSONObject* o=dynamic_cast<TJSONObject*>(root.get());
  const wchar_t* fields[]={L"version",L"favorites",L"home",L"selectedTerritory",L"view",L"detailLevel",L"showLabels",L"showBorders",L"showStatuses",L"theme",L"brightness",L"borderWidth"};
  if(!Keys(o,fields,12))return false;
  double n;if(!ReadNumber(o,L"version",n)||n!=1)return false;
  TJSONArray* favorites=dynamic_cast<TJSONArray*>(o->GetValue(UnicodeString(L"favorites")));
  if(!favorites||favorites->Count>20)return false;
  std::set<std::string> keys;
  for(int i=0;i<favorites->Count;++i){TJSONString* v=dynamic_cast<TJSONString*>(favorites->Items[i]);if(!v)return false;UTF8String text(v->Value());std::string key(text.c_str(),text.Length());if(!IsTerritoryKey(key)||!keys.insert(key).second)return false;}
  std::string home,selection,detail,theme;
  if(!ReadText(o,L"home",home)||(!home.empty()&&!keys.count(home))||!ReadText(o,L"selectedTerritory",selection)||(!selection.empty()&&!IsTerritoryKey(selection)))return false;
  if(!ReadText(o,L"detailLevel",detail)||(detail!="auto"&&detail!="State"&&detail!="District"&&detail!="Community"))return false;
  if(!ReadText(o,L"theme",theme)||(theme!="muted"&&theme!="graphite"&&theme!="midnight"))return false;
  const wchar_t* booleans[]={L"showLabels",L"showBorders",L"showStatuses"};
  for(int i=0;i<3;++i)if(!dynamic_cast<TJSONBool*>(o->GetValue(UnicodeString(booleans[i]))))return false;
  if(!ReadNumber(o,L"brightness",n)||n<60||n>120||floor(n)!=n)return false;
  if(!ReadNumber(o,L"borderWidth",n)||n<60||n>160||floor(n)!=n)return false;
  TJSONObject* view=dynamic_cast<TJSONObject*>(o->GetValue(UnicodeString(L"view")));
  const wchar_t* coordinates[]={L"x",L"y",L"w",L"h"};double x,y,w,h;
  if(!Keys(view,coordinates,4)||!ReadNumber(view,L"x",x)||!ReadNumber(view,L"y",y)||!ReadNumber(view,L"w",w)||!ReadNumber(view,L"h",h)||x<0||y<0||w<31.25||w>1000||fabs(h-w*0.7)>0.01||x+w>1000.01||y+h>700.01)return false;
  UTF8String text(o->ToJSON());normalized=std::string(text.c_str(),text.Length());return true;
 }catch(...){return false;}
}
