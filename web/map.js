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
 let preferences=PreferencesModel.defaults(),localSelection=false,favoritesRenderKey='';
 const stateShapes=new Map((geometry?.states||[]).map(s=>[s.oblast,s]));
 const kindName={State:'Область / місто',District:'Район',Community:'Громада'};
 function selectedKey(){return detailKey||(stateShapes.has(selected)?'State:'+stateShapes.get(selected).code:'');}
 function preferenceChange(patch,immediate=true){preferences={...preferences,...patch};window.appBridge?.preferences(patch,immediate);renderFavorites();}
 function saveView(immediate=false){preferenceChange({view:{...box}},immediate);}
 function chooseKey(key,send=true){const shape=shapes.get(key);if(!shape)return;localSelection=true;if(shape.kind==='State')select(shape.oblast,send);else chooseDetail(key,send);}
 function renderFavorites(){
  const current=selectedKey(),favoriteStates=preferences.favorites.map(key=>{const shape=shapes.get(key);return shape?currentStatus(shape):{level:'?'};}),nextKey=JSON.stringify([current,preferences.favorites,preferences.home,favoriteStates]);if(nextKey===favoritesRenderKey)return;favoritesRenderKey=nextKey;
  const isFavorite=preferences.favorites.includes(current);
  $('favorites-count').textContent=preferences.favorites.length+' / 20';$('favorites-empty').hidden=preferences.favorites.length>0;
  $('favorite-toggle').textContent=isFavorite?'★ Прибрати з моїх':'☆ Додати до моїх';$('favorite-toggle').disabled=!current||!isFavorite&&preferences.favorites.length>=20;
  $('home-toggle').disabled=!isFavorite;$('home-toggle').textContent=preferences.home===current?'Домашній регіон ✓':'Зробити домашнім';$('go-home').disabled=!shapes.has(preferences.home);
  const cards=preferences.favorites.map(key=>{const shape=shapes.get(key),card=document.createElement('div');card.className='favorite-card';card.dataset.key=key;
   const button=document.createElement('button');button.className='favorite-select';button.disabled=!shape;
   const title=document.createElement('strong');title.textContent=(preferences.home===key?'⌂ ':'')+(shape?.name||key);
   const subtitle=document.createElement('small');subtitle.textContent=shape?kindName[shape.kind]+' · '+data[shape.oblast].name:'Території немає в поточній версії меж';
   const status=document.createElement('span'),value=shape?currentStatus(shape):{level:'?'};status.className='favorite-status '+color(value.level);status.textContent=DetailModel.text(value)+(shape&&shape.kind!=='State'&&value.precision==='State'?' · дані області':'');
   button.append(title,subtitle,status);button.onclick=()=>{chooseKey(key);focusPath(detailPaths.get(key)||paths[shape.oblast]);saveView(true);};
   const remove=document.createElement('button');remove.className='favorite-remove';remove.textContent='×';remove.setAttribute('aria-label','Прибрати '+(shape?.name||key)+' з моїх');remove.onclick=()=>preferenceChange({favorites:preferences.favorites.filter(k=>k!==key),home:preferences.home===key?'':preferences.home});card.append(button,remove);return card;});
  $('favorites-list').replaceChildren(...cards);
 }
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
 for(const shape of geometry?.states||[])shapes.set('State:'+shape.code,shape);
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
  if(!keepDetail&&(selected!==index||send))detailKey='';
  if(send)localSelection=true;
  selected=index;$('region-select').value=String(index);$('region-name').textContent=data[index].name;
  paths.forEach((p,i)=>p.classList.toggle('selected',i===index));
  $('oblast-borders').querySelectorAll('path').forEach((p,i)=>p.classList.toggle('selected',i===index));
  if(optionRegion!==selected){
   optionRegion=selected;const placeholder=document.createElement('option');placeholder.value='';placeholder.textContent='Оберіть район або громаду';
   const options=[...shapes].filter(([,s])=>s.oblast===selected&&s.kind!=='State').sort((a,b)=>a[1].kind.localeCompare(b[1].kind)||a[1].name.localeCompare(b[1].name,'uk')).map(([key,s])=>{const o=document.createElement('option');o.value=key;o.textContent=(s.kind==='District'?'Район · ':'Громада · ')+s.name;return o;});
   $('territory-select').replaceChildren(placeholder,...options);
  }
  $('territory-select').value=detailKey;render();if(send){preferenceChange({selectedTerritory:selectedKey()});window.appBridge?.select(selected);}
 }
 function viewport(){
  svg.setAttribute('viewBox',`${box.x} ${box.y} ${box.w} ${box.h}`);$('scale').textContent=Math.round(1000/box.w*100)+'%';
  activeLayer=DetailModel.layer($('detail-level').value,1000/box.w,detailed());
  svg.dataset.detail=activeLayer;
  $('detail-hint').textContent=detailed()?`Кольори — стани громад. Межі та вибір: ${{State:'області',District:'райони',Community:'громади'}[activeLayer]}.`:'Це джерело / режим надає лише стани областей.';
 }
 function point(event){const p=svg.createSVGPoint();p.x=event.clientX;p.y=event.clientY;return p.matrixTransform(svg.getScreenCTM().inverse());}
 function zoom(amount,p={x:box.x+box.w/2,y:box.y+box.h/2}){box=MapModel.zoom(box,amount,p.x,p.y,bounds);viewport();}
 function currentStatus(shape){if(shape.kind==='State')return {level:MapModel.levels(observed?.levels,fresh())[shape.oblast],precision:'State',severityKnown:records.get(shape.apiId)?.severityKnown};return DetailModel.status(shape,records,MapModel.levels(observed?.levels,fresh()),fresh()&&!observed?.demo);}
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
    const shape=shapes.get(key),s=currentStatus(shape);path.dataset.level=s.level;path.dataset.context=s.context&&s.level==='P'?'oblast':'';
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
  viewport();renderDetail();renderFavorites();
 }
 window.airAlert={
  selectRegion:index=>{localSelection=true;select(index,false);preferenceChange({selectedTerritory:selectedKey()});},selectedRegion:()=>selected,selectedTerritory:()=>selectedKey(),
  setPreferences(value,restore=false){preferences=value;
   if(restore){box={...value.view};$('detail-level').value=value.detailLevel;for(const [id,k] of [['show-labels','showLabels'],['show-borders','showBorders'],['show-statuses','showStatuses']])$(id).checked=value[k];
    $('labels').style.display=value.showLabels?'':'none';svg.classList.toggle('no-borders',!value.showBorders);svg.classList.toggle('no-colors',!value.showStatuses);
    if(value.selectedTerritory&&shapes.has(value.selectedTerritory))chooseKey(value.selectedTerritory,false);localSelection=true;
   }viewport();renderFavorites();
  },
  update(value){if(!value||typeof value!=='object')return;observed=value;const revision=[value.provider,value.at,value.demo,value.available,value.detailRevision].join('|');if(revision!==recordRevision||Array.isArray(value.territories)){records=value.provider==='ukrainealarm'&&!value.demo?DetailModel.index(value.territories):new Map();recordRevision=revision;renderKey='';}if(!localSelection)select(value.selected,false,true);else render();}
 };
 $('favorite-toggle').onclick=()=>{const key=selectedKey();if(!key)return;if(preferences.favorites.includes(key))preferenceChange({favorites:preferences.favorites.filter(k=>k!==key),home:preferences.home===key?'':preferences.home});else if(preferences.favorites.length<20)preferenceChange({favorites:[...preferences.favorites,key]});};
 $('home-toggle').onclick=()=>{const key=selectedKey();if(preferences.favorites.includes(key))preferenceChange({home:preferences.home===key?'':key});};
 $('go-home').onclick=()=>{const shape=shapes.get(preferences.home);if(!shape)return;chooseKey(preferences.home);focusPath(detailPaths.get(preferences.home)||paths[shape.oblast]);saveView(true);};
 $('region-select').addEventListener('change',e=>select(Number(e.target.value)));
 $('territory-select').onchange=e=>{if(e.target.value)chooseDetail(e.target.value);else{detailKey='';select(selected);}};
 $('detail-level').onchange=()=>{viewport();preferenceChange({detailLevel:$('detail-level').value});};
 $('zoom-in').onclick=()=>{zoom(1.4);saveView(true);};$('zoom-out').onclick=()=>{zoom(1/1.4);saveView(true);};
 $('reset').onclick=()=>{box={...bounds};viewport();saveView(true);};
 function focusPath(path){const b=path.getBBox(),w=Math.max(b.width,b.height*bounds.w/bounds.h)*1.4;box=MapModel.zoom(bounds,bounds.w/Math.max(bounds.w/32,w),b.x+b.width/2,b.y+b.height/2,bounds);box=MapModel.pan({...box,x:b.x+b.width/2-box.w/2,y:b.y+b.height/2-box.h/2},0,0,bounds);viewport();}
 $('focus-region').onclick=()=>{focusPath(detailPaths.get(detailKey)||paths[selected]);saveView(true);};
 svg.addEventListener('wheel',event=>{event.preventDefault();zoom(event.deltaY<0?1.15:1/1.15,point(event));saveView();},{passive:false});
 svg.addEventListener('pointerdown',e=>{if(e.button!==0)return;e.preventDefault();window.getSelection()?.removeAllRanges();svg.focus({preventScroll:true});drag={x:e.clientX,y:e.clientY,box:{...box},moved:false,target:e.target.closest('path[data-region]')};svg.setPointerCapture(e.pointerId);svg.classList.add('dragging');});
 svg.addEventListener('pointermove',e=>{if(!drag)return;const dx=e.clientX-drag.x,dy=e.clientY-drag.y;drag.moved=drag.moved||Math.hypot(dx,dy)>5;const matrix=svg.getScreenCTM();box=MapModel.pan(drag.box,-dx/matrix.a,-dy/matrix.d,bounds);viewport();saveView();});
 function pick(path){if(path?.dataset.territory)chooseDetail(path.dataset.territory);else if(path)select(Number(path.dataset.region));}
 svg.addEventListener('pointerup',()=>{if(drag&&!drag.moved)pick(drag.target);drag=null;svg.classList.remove('dragging');saveView(true);});
 svg.addEventListener('pointercancel',()=>{drag=null;svg.classList.remove('dragging');saveView(true);});
 svg.addEventListener('dblclick',e=>{const p=document.elementFromPoint(e.clientX,e.clientY)?.closest('path[data-region]');if(p){pick(p);focusPath(p);saveView(true);}});
 svg.addEventListener('keydown',e=>{if(e.key==='+'||e.key==='=')zoom(1.4);else if(e.key==='-')zoom(1/1.4);else if(e.key==='0')$('reset').click();else if(e.key.startsWith('Arrow')){const d=box.w/10;box=MapModel.pan(box,e.key==='ArrowLeft'?-d:e.key==='ArrowRight'?d:0,e.key==='ArrowUp'?-d:e.key==='ArrowDown'?d:0,bounds);viewport();}else return;e.preventDefault();saveView();});
 $('show-labels').onchange=e=>{$('labels').style.display=e.target.checked?'':'none';preferenceChange({showLabels:e.target.checked});};
 $('show-borders').onchange=e=>{svg.classList.toggle('no-borders',!e.target.checked);preferenceChange({showBorders:e.target.checked});};
 $('show-statuses').onchange=e=>{svg.classList.toggle('no-colors',!e.target.checked);preferenceChange({showStatuses:e.target.checked});};
 select(7,false);viewport();setInterval(render,1000);
})();
