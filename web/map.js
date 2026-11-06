'use strict';
(() => {
 const $=id=>document.getElementById(id),svg=$('map'),ns='http://www.w3.org/2000/svg';
 const geometry=window.AIRALERT_TERRITORIES,regions=window.AIRALERT_REGIONS;
 const data=regions.map(r=>({...r,rings:geometry?.states.find(s=>s.oblast===r.index)?.rings||r.rings}));
 const all=data.flatMap(r=>r.rings.flat());
 let minX=Infinity,minY=Infinity,maxX=-Infinity,maxY=-Infinity;
 for(const p of all){minX=Math.min(minX,p[0]);minY=Math.min(minY,p[1]);maxX=Math.max(maxX,p[0]);maxY=Math.max(maxY,p[1]);}
 const factor=Math.min(930/(maxX-minX),625/(maxY-minY));
 const project=p=>[(p[0]-minX)*factor+35,(p[1]-minY)*factor+35];
 const bounds={x:0,y:0,w:1000,h:700};
 let box={...bounds},selected=7,detailKey='',observed=null,records=new Map(),drag=null,renderKey='',recordRevision='',activeLayer='State',optionRegion=-1;
 const shapes=new Map(),detailPaths=new Map();
 const makePath=(shape,group)=>{
  const path=document.createElementNS(ns,'path');
  path.setAttribute('d',shape.rings.map(r=>'M'+r.map(p=>project(p).map(v=>v.toFixed(2)).join(',')).join('L')+'Z').join(''));
  path.dataset.region=shape.oblast;path.dataset.level='?';
  const title=document.createElementNS(ns,'title');title.textContent=shape.name;path.append(title);
  $(group).append(path);return path;
 };
 const paths=data.map(region=>{
  const path=makePath({...region,oblast:region.index},'regions');
  path.setAttribute('aria-label',region.name);
  const border=path.cloneNode(true);border.classList.add('oblast-boundary');border.removeAttribute('aria-label');$('oblast-borders').append(border);
  const text=document.createElementNS(ns,'text'),p=project(region.label);
  text.setAttribute('x',p[0]);text.setAttribute('y',p[1]);text.textContent=region.shortName;$('labels').append(text);
  const option=document.createElement('option');option.value=region.index;option.textContent=region.name;$('region-select').append(option);
  return path;
 });
 for(const shape of [...(geometry?.communities||[]),...(geometry?.districts||[])]){
  const key=shape.kind+':'+shape.code;shapes.set(key,shape);
  const path=makePath(shape,shape.kind==='Community'?'communities':'districts');path.dataset.territory=key;
  detailPaths.set(key,path);
 }
 function fresh(){const age=Date.now()/1000-(observed?.at||0);return !!(observed&&(observed.demo||(observed.available&&age>=-5&&age<90)));}
 function detailed(){return !!(geometry&&observed?.provider==='ukrainealarm'&&!observed.demo&&records.size);}
 function chooseDetail(key,send=true){
  const shape=shapes.get(key);if(!shape)return;
  detailKey=key;select(shape.oblast,send,true);$('territory-select').value=key;renderDetail();
 }
 function select(index,send=true,keepDetail=false){
  if(!Number.isInteger(index)||index<0||index>=27)return;
  if(selected!==index||!keepDetail&&send)detailKey='';
  selected=index;$('region-select').value=String(index);$('region-name').textContent=data[index].name;
  paths.forEach((p,i)=>p.classList.toggle('selected',i===index));
  $('oblast-borders').querySelectorAll('path').forEach((p,i)=>p.classList.toggle('selected',i===index));
  if(optionRegion!==selected){
   optionRegion=selected;const placeholder=document.createElement('option');placeholder.value='';placeholder.textContent='Оберіть район або громаду';
   const options=[...shapes].filter(([,s])=>s.oblast===selected).sort((a,b)=>a[1].kind.localeCompare(b[1].kind)||a[1].name.localeCompare(b[1].name,'uk')).map(([key,s])=>{const o=document.createElement('option');o.value=key;o.textContent=(s.kind==='District'?'Район · ':'Громада · ')+s.name;return o;});
   $('territory-select').replaceChildren(placeholder,...options);
  }
  $('territory-select').value=detailKey;render();if(send)window.appBridge?.select(selected);
 }
 function viewport(){
  svg.setAttribute('viewBox',`${box.x} ${box.y} ${box.w} ${box.h}`);$('scale').textContent=Math.round(1000/box.w*100)+'%';
  activeLayer=DetailModel.layer($('detail-level').value,1000/box.w,detailed());
  svg.dataset.detail=activeLayer;
  $('detail-hint').textContent=detailed()?`Кольори — стани громад. Межі та вибір: ${{State:'області',District:'райони',Community:'громади'}[activeLayer]}.`:'Це джерело / режим надає лише стани областей.';
 }
 function point(event){const p=svg.createSVGPoint();p.x=event.clientX;p.y=event.clientY;return p.matrixTransform(svg.getScreenCTM().inverse());}
 function zoom(amount,p={x:box.x+box.w/2,y:box.y+box.h/2}){box=MapModel.zoom(box,amount,p.x,p.y,bounds);viewport();}
 function currentStatus(shape){return DetailModel.status(shape,records,MapModel.levels(observed?.levels,fresh()),fresh()&&!observed?.demo);}
 function renderDetail(){
  const shape=shapes.get(detailKey);$('territory-card').hidden=!shape;
  detailPaths.forEach((p,key)=>p.classList.toggle('selected',key===detailKey));
  if(!shape)return;
  const status=currentStatus(shape);$('territory-name').textContent=shape.name;
  const el=$('territory-status');el.textContent=DetailModel.text(status);el.className='status '+color(status.level);
  const source=records.get(status.source);
  $('territory-source').textContent=DetailModel.explanation(status,shape)||(source&&source.id!==shape.apiId?'Тривога поширюється з: '+source.name:shape.kind==='District'&&status.level==='P'?'Тривога в окремих громадах. Наблизьте мапу для точних меж.':'Статус території за її regionId у довіднику UkraineAlarm.');
 }
 function color(level){return level==='R'?'red':level==='N'?'clear':level==='Y'?'amber':level==='P'?'partial':'unknown';}
 function render(){
  const isFresh=fresh(),levels=MapModel.levels(observed?.levels,isFresh),key=[selected,observed?.at,isFresh,observed?.demo,levels,records.size].join('|');
  if(key!==renderKey){
   renderKey=key;
   paths.forEach((p,i)=>{p.dataset.level=levels[i];p.firstChild.textContent=data[i].name+' — '+DetailModel.text({level:levels[i]});});
   let mapped=0,total=0;
   for(const [key,path] of detailPaths){
    const shape=shapes.get(key),s=currentStatus(shape);path.dataset.level=s.level;
    path.firstChild.textContent=shape.name+' — '+DetailModel.text(s)+(s.precision==='State'?' (дані області)':'');
    if(shape.kind==='Community'&&shape.oblast===selected){total++;if(s.precision==='Community')mapped++;}
   }
   $('detail-coverage').textContent=detailed()?`Деталізація: ${mapped} із ${total} громад обраної області зіставлено з API. Для решти — лише загальний стан області або «немає даних».`:'Детальні стани громад доступні з UkraineAlarm. Для alerts.in.ua та деморежиму показано області.';
  }
  $('communities').style.display=detailed()?'':'none';$('districts').style.display=detailed()?'':'none';
  $('territory-select').disabled=!detailed();$('detail-level').disabled=!detailed();
  const level=levels[selected],status=$('region-status');status.textContent=level==='P'?'Тривога в окремих районах / громадах':DetailModel.text({level,severityKnown:records.size>0&&[...records.values()].some(t=>t.oblast===selected&&t.type==='State'&&t.level===level&&t.severityKnown)});status.className='status '+color(level);
  if(observed?.provider==='alerts.in.ua'&&level==='Y')status.textContent='Попередня загроза';
  const explanation=$('region-explanation');explanation.hidden=!(level==='?'&&isFresh&&observed?.provider==='ukrainealarm');
  explanation.textContent=selected===26?'Севастополя немає в довіднику UkraineAlarm. Дані Криму не підміняють окремий статус міста.':'Постачальник не підтвердив окремий статус цього регіону.';
  const active=[...levels].filter(x=>'RPY'.includes(x)).length,known=[...levels].filter(x=>x!=='?').length;
  $('counts').textContent=`${active} із тривогою / загрозою · ${27-known} без даних`;
  $('source').textContent='Джерело: '+(observed?.provider==='ukrainealarm'?'UkraineAlarm / Stfalcon':observed?.provider==='alerts.in.ua'?'alerts.in.ua':'ще не підключено');
  $('updated').textContent=observed?.at?'Останнє підтвердження: '+new Date(observed.at*1000).toLocaleString('uk-UA'):'Підтвердження ще не отримано';
  viewport();renderDetail();
 }
 window.airAlert={update(value){if(!value||typeof value!=='object')return;observed=value;const revision=[value.provider,value.at,value.demo,value.available,value.detailRevision].join('|');if(revision!==recordRevision||Array.isArray(value.territories)){records=value.provider==='ukrainealarm'&&!value.demo?DetailModel.index(value.territories):new Map();recordRevision=revision;renderKey='';}select(value.selected,false,true);}};
 $('region-select').addEventListener('change',e=>select(Number(e.target.value)));
 $('territory-select').onchange=e=>{if(e.target.value)chooseDetail(e.target.value);else{detailKey='';renderDetail();}};
 $('detail-level').onchange=viewport;
 $('zoom-in').onclick=()=>zoom(1.4);$('zoom-out').onclick=()=>zoom(1/1.4);
 $('reset').onclick=()=>{box={...bounds};viewport();};
 function focusPath(path){const b=path.getBBox(),w=Math.max(b.width,b.height*bounds.w/bounds.h)*1.4;box=MapModel.zoom(bounds,bounds.w/Math.max(bounds.w/32,w),b.x+b.width/2,b.y+b.height/2,bounds);box=MapModel.pan({...box,x:b.x+b.width/2-box.w/2,y:b.y+b.height/2-box.h/2},0,0,bounds);viewport();}
 $('focus-region').onclick=()=>focusPath(detailPaths.get(detailKey)||paths[selected]);
 svg.addEventListener('wheel',event=>{event.preventDefault();zoom(event.deltaY<0?1.15:1/1.15,point(event));},{passive:false});
 svg.addEventListener('pointerdown',e=>{if(e.button!==0)return;e.preventDefault();window.getSelection()?.removeAllRanges();svg.focus({preventScroll:true});drag={x:e.clientX,y:e.clientY,box:{...box},moved:false,target:e.target.closest('path[data-region]')};svg.setPointerCapture(e.pointerId);svg.classList.add('dragging');});
 svg.addEventListener('pointermove',e=>{if(!drag)return;const dx=e.clientX-drag.x,dy=e.clientY-drag.y;drag.moved=drag.moved||Math.hypot(dx,dy)>5;const matrix=svg.getScreenCTM();box=MapModel.pan(drag.box,-dx/matrix.a,-dy/matrix.d,bounds);viewport();});
 function pick(path){if(path?.dataset.territory)chooseDetail(path.dataset.territory);else if(path)select(Number(path.dataset.region));}
 svg.addEventListener('pointerup',()=>{if(drag&&!drag.moved)pick(drag.target);drag=null;svg.classList.remove('dragging');});
 svg.addEventListener('pointercancel',()=>{drag=null;svg.classList.remove('dragging');});
 svg.addEventListener('dblclick',e=>{const p=document.elementFromPoint(e.clientX,e.clientY)?.closest('path[data-region]');if(p){pick(p);focusPath(p);}});
 svg.addEventListener('keydown',e=>{if(e.key==='+'||e.key==='=')zoom(1.4);else if(e.key==='-')zoom(1/1.4);else if(e.key==='0')$('reset').click();else if(e.key.startsWith('Arrow')){const d=box.w/10;box=MapModel.pan(box,e.key==='ArrowLeft'?-d:e.key==='ArrowRight'?d:0,e.key==='ArrowUp'?-d:e.key==='ArrowDown'?d:0,bounds);viewport();}else return;e.preventDefault();});
 $('show-labels').onchange=e=>{$('labels').style.display=e.target.checked?'':'none';};
 $('show-borders').onchange=e=>svg.classList.toggle('no-borders',!e.target.checked);
 $('show-statuses').onchange=e=>svg.classList.toggle('no-colors',!e.target.checked);
 select(7,false);viewport();setInterval(render,1000);
})();
