'use strict';
(() => {
 const $=id=>document.getElementById(id), svg=$('map'), ns='http://www.w3.org/2000/svg';
 const data=window.AIRALERT_REGIONS;
 const all=data.flatMap(r=>r.rings.flat()), minX=Math.min(...all.map(p=>p[0])),minY=Math.min(...all.map(p=>p[1]));
 const rangeX=Math.max(...all.map(p=>p[0]))-minX,rangeY=Math.max(...all.map(p=>p[1]))-minY;
 const factor=Math.min(930/rangeX,625/rangeY), project=p=>[(p[0]-minX)*factor+35,(p[1]-minY)*factor+35];
 const bounds={x:0,y:0,w:1000,h:700}; let box={...bounds},selected=7,observed=null,drag=null;
 const texts={N:'Немає активної тривоги',R:'Тривога в усій області',P:'Часткова тривога',Y:'Попередня загроза','?':'Немає даних'};
 const paths=data.map(region=>{
  const path=document.createElementNS(ns,'path');
  path.setAttribute('d',region.rings.map(r=>'M'+r.map(p=>project(p).join(',')).join('L')+'Z').join(''));
  path.dataset.region=region.index;path.dataset.level='?';path.setAttribute('aria-label',region.name);
  const title=document.createElementNS(ns,'title');title.textContent=region.name;path.append(title);$('regions').append(path);
  const text=document.createElementNS(ns,'text'),p=project(region.label);
  text.setAttribute('x',p[0]);text.setAttribute('y',p[1]);text.textContent=region.shortName;$('labels').append(text);
  const option=document.createElement('option');option.value=region.index;option.textContent=region.name;$('region-select').append(option);
  return path;
 });
 function notify() {window.appBridge?.select(selected);}
 function select(index,send=true) {
  if(!Number.isInteger(index)||index<0||index>=27)return;
  selected=index;$('region-select').value=String(index);$('region-name').textContent=data[index].name;
  paths.forEach((p,i)=>p.classList.toggle('selected',i===index));render();if(send)notify();
 }
 function viewport() {svg.setAttribute('viewBox',`${box.x} ${box.y} ${box.w} ${box.h}`);$('scale').textContent=Math.round(1000/box.w*100)+'%';}
 function point(event) {const p=svg.createSVGPoint();p.x=event.clientX;p.y=event.clientY;return p.matrixTransform(svg.getScreenCTM().inverse());}
 function zoom(amount,p={x:box.x+box.w/2,y:box.y+box.h/2}) {box=MapModel.zoom(box,amount,p.x,p.y,bounds);viewport();}
 function render() {
  const fresh=observed&&(observed.demo||(observed.available&&Date.now()/1000-observed.at<90&&Date.now()/1000-observed.at>=-5));
  const levels=MapModel.levels(observed?.levels,!!fresh);
  paths.forEach((p,i)=>{p.dataset.level=levels[i];p.firstChild.textContent=data[i].name+' — '+texts[levels[i]];});
  const level=levels[selected],status=$('region-status');status.textContent=texts[level];status.className='status '+(level==='R'?'red':level==='N'?'clear':level==='?'?'unknown':'amber');
  const active=[...levels].filter(x=>'RPY'.includes(x)).length,known=[...levels].filter(x=>x!=='?').length;
  $('counts').textContent=`${active} із тривогою / загрозою · ${27-known} без даних`;
  $('source').textContent='Джерело: '+(observed?.provider==='ukrainealarm'?'UkraineAlarm / Stfalcon':observed?.provider==='alerts.in.ua'?'alerts.in.ua':'ще не підключено');
  $('updated').textContent=observed?.at?'Останнє підтвердження: '+new Date(observed.at*1000).toLocaleString('uk-UA'):'Підтвердження ще не отримано';
 }
 window.airAlert={update(value){if(!value||typeof value!=='object')return;observed=value;select(value.selected,false);render();}};
 $('region-select').addEventListener('change',e=>select(Number(e.target.value)));
 $('zoom-in').onclick=()=>zoom(1.4);$('zoom-out').onclick=()=>zoom(1/1.4);
 $('reset').onclick=()=>{box={...bounds};viewport();};
 function focusRegion(){const b=paths[selected].getBBox(),w=Math.max(b.width,b.height*bounds.w/bounds.h)*1.6;box=MapModel.zoom(bounds,bounds.w/Math.max(125,w),b.x+b.width/2,b.y+b.height/2,bounds);box=MapModel.pan({...box,x:b.x+b.width/2-box.w/2,y:b.y+b.height/2-box.h/2},0,0,bounds);viewport();}
 $('focus-region').onclick=focusRegion;
 svg.addEventListener('wheel',event=>{event.preventDefault();zoom(event.deltaY<0?1.15:1/1.15,point(event));},{passive:false});
 svg.addEventListener('pointerdown',e=>{if(e.button!==0)return;drag={x:e.clientX,y:e.clientY,box:{...box},moved:false,target:e.target.closest('path')};svg.setPointerCapture(e.pointerId);svg.classList.add('dragging');});
 svg.addEventListener('pointermove',e=>{if(!drag)return;const dx=e.clientX-drag.x,dy=e.clientY-drag.y;drag.moved=drag.moved||Math.hypot(dx,dy)>5;const matrix=svg.getScreenCTM();box=MapModel.pan(drag.box,-dx/matrix.a,-dy/matrix.d,bounds);viewport();});
 svg.addEventListener('pointerup',()=>{if(drag&&!drag.moved&&drag.target)select(Number(drag.target.dataset.region));drag=null;svg.classList.remove('dragging');});
 svg.addEventListener('pointercancel',()=>{drag=null;svg.classList.remove('dragging');});
 svg.addEventListener('dblclick',e=>{const p=document.elementFromPoint(e.clientX,e.clientY)?.closest('path[data-region]');if(p){select(Number(p.dataset.region));focusRegion();}});
 svg.addEventListener('keydown',e=>{if(e.key==='+'||e.key==='=')zoom(1.4);else if(e.key==='-')zoom(1/1.4);else if(e.key==='0'){$('reset').click();}else if(e.key.startsWith('Arrow')){const d=box.w/10;box=MapModel.pan(box,e.key==='ArrowLeft'?-d:e.key==='ArrowRight'?d:0,e.key==='ArrowUp'?-d:e.key==='ArrowDown'?d:0,bounds);viewport();}else return;e.preventDefault();});
 $('show-labels').onchange=e=>{$('labels').style.display=e.target.checked?'':'none';};
 $('show-borders').onchange=e=>svg.classList.toggle('no-borders',!e.target.checked);
 $('show-statuses').onchange=e=>svg.classList.toggle('no-colors',!e.target.checked);
 select(7,false);viewport();setInterval(render,1000);
})();
