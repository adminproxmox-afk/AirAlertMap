#include "UkraineAlarmFeed.h"
#include "MapData.h"
#include <System.JSON.hpp>
#include <System.SysUtils.hpp>
#include <memory>
#include <map>

using namespace System;
using namespace System::Json;



static UnicodeString Field(TJSONObject* object,const wchar_t* name) {
    TJSONString* value=dynamic_cast<TJSONString*>(object->GetValue(UnicodeString(name)));
    return value?value->Value():UnicodeString();
}
static int MapIndex(UnicodeString name) {
    name=System::Sysutils::AnsiLowerCase(name.Trim());
    const UnicodeString suffix=L"\u043e\u0431\u043b\u0430\u0441\u0442\u044c";
    if(name.Length()>=suffix.Length() && name.SubString(name.Length()-suffix.Length()+1,suffix.Length())==suffix)
        name=name.SubString(1,name.Length()-suffix.Length()).Trim();
    for(int i=0;i<RegionCount;++i) {
        UnicodeString canonical=System::Sysutils::AnsiLowerCase(UnicodeString(Regions[i].name));
        if(canonical.Length()>=suffix.Length() && canonical.SubString(canonical.Length()-suffix.Length()+1,suffix.Length())==suffix)
            canonical=canonical.SubString(1,canonical.Length()-suffix.Length()).Trim();
        if(name==canonical) return i;
    }
    for(int i=0;i<RegionCount;++i)
        if(System::Sysutils::SameText(name,UnicodeString(Regions[i].name))) return i;
    if(name.SubString(1,2)==L"\u043c.") name=name.SubString(3,name.Length()).Trim();
    if(name==L"\u043a\u0438\u0457\u0432") return 25;
    if(name==L"\u0441\u0435\u0432\u0430\u0441\u0442\u043e\u043f\u043e\u043b\u044c") return 26;
    if(name==L"\u0430\u0440 \u043a\u0440\u0438\u043c" || name==L"\u043a\u0440\u0438\u043c") return 18;
    return -1;
}

struct RegionBinding {
    int mapIndex;
    bool wholeRegion;
    RegionBinding() : mapIndex(-1),wholeRegion(false) {}
    RegionBinding(int index,bool whole) : mapIndex(index),wholeRegion(whole) {}
};
typedef std::map<std::wstring,RegionBinding> RegionDirectory;

static TJSONValue* CopyPublicFields(TJSONValue* value,int depth) {
    if(depth>16) return new TJSONNull();
    TJSONArray* array=dynamic_cast<TJSONArray*>(value);
    if(array) {
        TJSONArray* result=new TJSONArray();
        for(int i=0;i<array->Count && i<20000;++i) result->AddElement(CopyPublicFields(array->Items[i],depth+1));
        return result;
    }
    TJSONObject* object=dynamic_cast<TJSONObject*>(value);
    if(object) {
        TJSONObject* result=new TJSONObject();
        const wchar_t* fields[]={L"states",L"regionId",L"regionName",L"regionType",L"regionChildIds",L"activeAlerts",L"type",L"level",L"alertLevel"};
        for(unsigned i=0;i<sizeof(fields)/sizeof(fields[0]);++i) {
            TJSONValue* field=object->GetValue(UnicodeString(fields[i]));
            if(field) result->AddPair(UnicodeString(fields[i]),CopyPublicFields(field,depth+1));
        }
        return result;
    }
    if(dynamic_cast<TJSONString*>(value)) return new TJSONString(value->Value());
    if(dynamic_cast<TJSONNumber*>(value)) return new TJSONString(value->Value());
    return new TJSONNull();
}
std::string PublicUkraineAlarmDiagnostic(const std::string& json) {
    try {
        std::auto_ptr<TJSONValue> root(TJSONObject::ParseJSONValue(UTF8String(json.c_str(),json.size()),false,false));
        if(!root.get()) return "";
        std::auto_ptr<TJSONValue> clean(CopyPublicFields(root.get(),0));
        UTF8String utf8(clean->ToJSON());
        return std::string(utf8.c_str(),utf8.Length());
    } catch(...) { return ""; }
}

static bool RegisterRegion(TJSONObject* node,int parent,int depth,RegionDirectory& ids,bool* covered) {
    if(!node || depth>16 || ids.size()>20000) return false;
    UnicodeString id=Field(node,L"regionId"),name=Field(node,L"regionName");
    if(id.IsEmpty() || name.IsEmpty()) return false;
    int own=MapIndex(name);
    bool whole=own>=0 && (depth==0 || Field(node,L"regionType")==L"State" || own==25 || own==26);
    int index=whole?own:parent;
    // UkraineAlarm also lists these cities as separate State roots in addition
    // to their entries within the oblast hierarchy. They cover part of an oblast.
    UnicodeString lower=System::Sysutils::AnsiLowerCase(name.Trim());
    if(depth==0 && own<0) {
        if(lower.Pos(L"\u043c. \u0445\u0430\u0440\u043a\u0456\u0432")==1) index=7;
        else if(lower.Pos(L"\u043c. \u0437\u0430\u043f\u043e\u0440\u0456\u0436\u0436\u044f")==1) index=10;
        else if(id==L"0" && lower.Pos(L"\u0442\u0435\u0441\u0442\u043e\u0432\u0438\u0439")==1) index=-2;
    }
    std::wstring key=id.c_str();
    RegionDirectory::iterator existing=ids.find(key);
    if(existing!=ids.end() && existing->second.mapIndex!=index) return false;
    ids[key]=RegionBinding(index,whole);
    if(whole) covered[index]=true;
    TJSONValue* children=node->GetValue(UnicodeString(L"regionChildIds"));
    if(!children || dynamic_cast<TJSONNull*>(children)) return true;
    TJSONArray* array=dynamic_cast<TJSONArray*>(children);
    if(!array) return false;
    for(int i=0;i<array->Count;++i)
        if(!RegisterRegion(dynamic_cast<TJSONObject*>(array->Items[i]),index,depth+1,ids,covered)) return false;
    return true;
}

bool ParseUkraineAlarmCoverage(const std::string& json,const std::string& directory,AlertSnapshot& result) {
    const unsigned maxBytes=2*1024*1024;
    if(json.empty() || directory.empty() || json.size()>maxBytes || directory.size()>maxBytes ||
       json.find('\0')!=std::string::npos || directory.find('\0')!=std::string::npos) return false;
    try {
        std::auto_ptr<TJSONValue> dirRoot(TJSONObject::ParseJSONValue(UTF8String(directory.c_str(),directory.size()),false,false));
        TJSONObject* object=dynamic_cast<TJSONObject*>(dirRoot.get());
        TJSONArray* states=object?dynamic_cast<TJSONArray*>(object->GetValue(UnicodeString(L"states"))):0;
        if(!states || states->Count==0 || states->Count>64) return false;
        RegionDirectory ids;
        bool covered[RegionCount]={false};
        for(int i=0;i<states->Count;++i)
            if(!RegisterRegion(dynamic_cast<TJSONObject*>(states->Items[i]),-1,0,ids,covered)) return false;
        AlertSnapshot parsed;
        int mapped=0;
        for(int i=0;i<RegionCount;++i) if(covered[i]) {
            parsed.states[i]=asClear; parsed.levels[i]='N'; ++mapped;
        }
        if(!mapped) return false;
        std::auto_ptr<TJSONValue> root(TJSONObject::ParseJSONValue(UTF8String(json.c_str(),json.size()),false,false));
        TJSONArray* regions=dynamic_cast<TJSONArray*>(root.get());
        if(!regions || regions->Count>20000) return false;
        for(int i=0;i<regions->Count;++i) {
            TJSONObject* region=dynamic_cast<TJSONObject*>(regions->Items[i]);
            if(!region) return false;
            TJSONArray* alerts=dynamic_cast<TJSONArray*>(region->GetValue(UnicodeString(L"activeAlerts")));
            if(!alerts) return false;
            for(int j=0;j<alerts->Count;++j) {
                TJSONObject* alert=dynamic_cast<TJSONObject*>(alerts->Items[j]);
                if(!alert) return false;
                UnicodeString type=Field(alert,L"type");
                if(type.IsEmpty()) return false;
                if(type!=L"AIR") continue;
                UnicodeString id=Field(alert,L"regionId");
                if(id.IsEmpty()) id=Field(region,L"regionId");
                RegionDirectory::const_iterator binding=ids.find(std::wstring(id.c_str()));
                if(binding!=ids.end() && binding->second.mapIndex==-2) continue;
                if(binding==ids.end() || binding->second.mapIndex<0) return false;
                int index=binding->second.mapIndex;
                char level=binding->second.wholeRegion?'R':'P';
                if(parsed.levels[index]!='R') parsed.levels[index]=level;
                parsed.states[index]=asAlert;
            }
        }
        result=parsed;
        return true;
    } catch(...) { return false; }
}

