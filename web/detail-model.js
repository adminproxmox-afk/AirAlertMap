(function(root) {
 'use strict';
 const api={
  index(value) {
   const result=new Map();
   if(!Array.isArray(value)||value.length>20000)return result;
   for(const t of value) {
    if(!t||typeof t.id!=='string'||typeof t.name!=='string'||typeof t.parent!=='string'||!Number.isInteger(t.oblast)||t.oblast<0||t.oblast>=27||!['N','R','Y','P','?'].includes(t.level)||result.has(t.id))return new Map();
    result.set(t.id,t);
   }
   return result;
  },
  status(shape,records,levels,fresh) {
   if(!fresh)return {level:'?',precision:'unknown'};
   const t=shape.apiId?records.get(shape.apiId):null;
   // IDs are provider-specific. Require the reviewed directory name and parent
   // as well: a changed directory cannot silently reuse an unrelated polygon.
   if(t&&t.name===shape.apiName&&t.parent===shape.apiParent&&t.oblast===shape.oblast)
    return {...t,precision:shape.kind};
   const level=levels[shape.oblast]||'?';
   // Full oblast coverage is meaningful even where the provider has no child
   // ID. A partial oblast gives no evidence about an unmapped child.
   return {level:level==='P'?'?':level,precision:'State',severityKnown:false,reason:shape.apiId?'directoryChanged':'notListed'};
  },
  layer(choice,scale,detailed) {
   if(!detailed)return 'State';
   if(choice!=='auto')return choice;
   return scale>=3.2?'Community':scale>=1.7?'District':'State';
  },
  text(status) {
   if(status.level==='N')return 'Немає активної повітряної тривоги';
   if(status.level==='P')return 'Тривога в частині території';
   if(status.level==='?')return status.reason?'Немає окремого статусу API':'Немає підтверджених даних';
   if(status.severityKnown)return status.level==='Y'?'Повітряна тривога · жовтий рівень':'Повітряна тривога · червоний рівень';
   return 'Повітряна тривога · рівень не вказано API';
  },
  explanation(status,shape) {
   if(status.precision==='unknown')return 'Статус не підтверджено свіжою відповіддю API. Перевірте підключення та час оновлення.';
   if(status.precision!=='State')return '';
   const reason=status.reason==='directoryChanged'?'Запис API змінив назву або ієрархію. Детальний статус не прив’язано до старої геометрії.':shape?.oblast===26?'Севастополя немає в довіднику цього API. Статус Криму не переноситься на місто.':shape?.code==='UA3200000'?'Чорнобильська зона відчуження не має окремого regionId у довіднику API.':'Постачальник не надає окремого перевіреного запису для цієї території.';
   return reason+(status.level==='?'?' Загальний частковий стан області не визначає статус цієї території.':' Показано підтверджений загальний стан усієї області.');
  }
 };
 if(typeof module==='object'&&module.exports)module.exports=api;else root.DetailModel=api;
})(typeof window==='object'?window:globalThis);
