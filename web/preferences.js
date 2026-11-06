(function(root){
 'use strict';
 const clone=v=>JSON.parse(JSON.stringify(v));
 const key=v=>typeof v==='string'&&v.length<=32&&/^(State|District|Community):UA[0-9]{2,9}$/.test(v);
 const defaults=()=>({version:1,favorites:[],home:'',selectedTerritory:'',view:{x:0,y:0,w:1000,h:700},detailLevel:'auto',showLabels:true,showBorders:true,showStatuses:true,theme:'muted',brightness:100,borderWidth:100});
 function valid(p){
  const v=p?.view;
  return !!(p&&p.version===1&&Array.isArray(p.favorites)&&p.favorites.length<=20&&p.favorites.every(key)&&new Set(p.favorites).size===p.favorites.length&&(p.home===''||p.favorites.includes(p.home))&&(p.selectedTerritory===''||key(p.selectedTerritory))&&v&&['x','y','w','h'].every(k=>Number.isFinite(v[k]))&&v.w>=31.25&&v.w<=1000&&Math.abs(v.h-v.w*.7)<.000001&&v.x>=0&&v.y>=0&&v.x+v.w<=1000.000001&&v.y+v.h<=700.000001&&['auto','State','District','Community'].includes(p.detailLevel)&&['showLabels','showBorders','showStatuses'].every(k=>typeof p[k]==='boolean')&&['muted','graphite','midnight'].includes(p.theme)&&Number.isInteger(p.brightness)&&p.brightness>=60&&p.brightness<=120&&Number.isInteger(p.borderWidth)&&p.borderWidth>=60&&p.borderWidth<=160);
 }
 function same(a,b){return Object.keys(defaults()).every(k=>k==='view'?['x','y','w','h'].every(v=>Math.abs(a.view[v]-b.view[v])<.00001):JSON.stringify(a[k])===JSON.stringify(b[k]));}
 function create(){
  let draft=defaults(),initialized=false,revision=-1,dirty=false,inflight=null;const touched=new Set();
  return {
   value:()=>clone(draft),
   set(patch){const next={...draft,...clone(patch)};if(!valid(next))return false;for(const k of Object.keys(patch))touched.add(k);if(JSON.stringify(next)!==JSON.stringify(draft)){draft=next;dirty=true;}return true;},
   prepare(){if(!initialized||!dirty||inflight)return null;dirty=false;inflight={revision,value:clone(draft)};return clone(draft);},
   cancel(){if(inflight){inflight=null;dirty=true;}},
   receive(state){
    const result={apply:null,error:'',acknowledged:false},nextRevision=state.preferencesRevision;
    if(!Number.isSafeInteger(nextRevision)||nextRevision<0||nextRevision<revision||!valid(state.preferences))return result;
    if(!initialized){const merged=clone(state.preferences);for(const k of touched)merged[k]=clone(draft[k]);if(valid(merged))draft=merged;initialized=true;result.apply=clone(draft);}
    if(inflight&&nextRevision>inflight.revision){
     if(state.preferencesError){inflight=null;result.acknowledged=true;result.error=String(state.preferencesError);}
     else if(same(state.preferences,inflight.value)){inflight=null;result.acknowledged=true;}
     // A changed state unrelated to this pending payload must not discard it.
    }
    revision=nextRevision;return result;
   }
  };
 }
 const api={defaults,valid,create};if(typeof module==='object'&&module.exports)module.exports=api;else root.PreferencesModel=api;
})(typeof window==='object'?window:globalThis);
