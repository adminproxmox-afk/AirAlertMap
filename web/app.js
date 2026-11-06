'use strict';
(() => {
 const $=id=>document.getElementById(id), regions=window.AIRALERT_REGIONS;
 const mapUpdate=window.airAlert.update.bind(window.airAlert);
 const titles={map:['Мапа тривог','ПОТОЧНА ОБСТАНОВКА'],history:['Історія спостережень','ЛОКАЛЬНА АНАЛІТИКА'],settings:['Налаштування','ВАШЕ ПІДКЛЮЧЕННЯ']};
 const labels={red:['Повна тривога','red'],partial:['Часткова','amber'],threat:['Попередня загроза','amber'],clear:['Без тривоги','clear'],unknown:['Немає даних','unknown']};
 let state={version:1,levels:'?'.repeat(27),selected:7,provider:'ukrainealarm',available:false,demo:false,keyConfigured:false,keySaved:false,remember:true,settingsRevision:0,historyDays:7},page='map',initialized=false,settingsDirty=false,pendingSave=false,lastRevision=-1,historyRenderKey='',toastTimer;
 function text(id,value){const element=$(id),next=String(value??'');if(element.textContent!==next)element.textContent=next;}
 function toast(message,error=false){text('action-notice',message);$('action-notice').classList.toggle('error',error);$('action-notice').hidden=false;clearTimeout(toastTimer);toastTimer=setTimeout(()=>$('action-notice').hidden=true,6500);}
 function send(type,payload={}) {
  if(!window.chrome?.webview?.postMessage){toast('Відкрийте AirAlertMap.exe, щоб застосувати цю дію.',true);return false;}
  try{window.chrome.webview.postMessage(JSON.stringify({...payload,version:1,type}));return true;}
  catch{toast('Зв’язок із програмою недоступний. Перезапустіть застосунок.',true);return false;}
 }
 function chooseRegion(index){if(!Number.isInteger(index)||index<0||index>=27)return;state={...state,selected:index};mapUpdate(state);syncRegion();historyRenderKey='';renderHistory();send('select',{region:index});}
 window.appBridge={select:chooseRegion};
 for(const id of ['history-region','settings-region'])regions.forEach(region=>{const option=document.createElement('option');option.value=region.index;option.textContent=region.name;$(id).append(option);});
 function syncRegion(){for(const id of ['history-region','settings-region'])$(id).value=String(state.selected);}
 function navigate(next){if(!titles[next])return;page=next;for(const name of Object.keys(titles))$('page-'+name).hidden=name!==next;document.querySelectorAll('[data-page]').forEach(button=>{const active=button.dataset.page===next;button.classList.toggle('active',active);if(active)button.setAttribute('aria-current','page');else button.removeAttribute('aria-current');});text('page-title',titles[next][0]);text('section-caption',titles[next][1]);document.title=titles[next][0]+' · AirAlertMap';text('footer-caption',next==='map'?'Межі: Kartographia / OCHA · CC BY 3.0 IGO':'AirAlertMap · Локальні спостереження');if(next==='history'){renderHistory();if(!state.historyLoading)send('history',{days:Number($('history-period').value)});}}
 document.querySelectorAll('[data-page]').forEach(button=>button.addEventListener('click',()=>navigate(button.dataset.page)));
 $('region-history').onclick=()=>navigate('history');
 $('history-region').onchange=e=>chooseRegion(Number(e.target.value));$('settings-region').onchange=e=>chooseRegion(Number(e.target.value));
 $('refresh-api').onclick=()=>send('refresh');
 document.querySelectorAll('[data-command]').forEach(button=>button.onclick=()=>send(button.dataset.command));
 $('live-mode').onchange=e=>{if(!send('mode',{live:e.target.checked}))e.target.checked=!state.demo;};
 $('history-period').onchange=()=>send('history',{days:Number($('history-period').value)});
 $('history-load').onclick=()=>send('history',{days:Number($('history-period').value)});
 document.querySelectorAll('[data-export]').forEach(button=>button.onclick=()=>send('export',{format:button.dataset.export}));
 for(const id of ['provider','api-token','remember-key'])$(id).addEventListener('input',()=>settingsDirty=true);
 $('show-token').onclick=()=>{const visible=$('api-token').type==='password';$('api-token').type=visible?'text':'password';$('show-token').setAttribute('aria-pressed',String(visible));text('show-token',visible?'Сховати':'Показати');};
 function formMessage(message,error){text('settings-message',message);$('settings-message').classList.toggle('error',error);}
 $('settings-form').addEventListener('submit',event=>{
  event.preventDefault();if(pendingSave)return;
  const token=$('api-token').value.trim(),provider=$('provider').value,remember=$('remember-key').checked;
  if(token&&(!/^[\x21-\x7e]{1,2048}$/.test(token))){formMessage('Ключ має бути без пробілів і переносів рядків.',true);return;}
  if(!token&&(!state.keyConfigured||provider!==state.provider)){formMessage('Введіть ключ для обраного постачальника.',true);return;}
  $('api-token').value='';$('api-token').type='password';text('show-token','Показати');$('show-token').setAttribute('aria-pressed','false');
  if(send('saveSettings',{token,provider,remember})){pendingSave=true;$('save-settings').disabled=true;formMessage('Застосування налаштувань…',false);}
 });
 $('forget-key').onclick=()=>$('forget-dialog').showModal();$('cancel-forget').onclick=()=>$('forget-dialog').close();$('confirm-forget').onclick=()=>{$('forget-dialog').close();$('api-token').value='';send('forgetKey',{confirm:true});};
 function duration(seconds){seconds=Math.max(0,Math.floor(Number(seconds)||0));const hours=Math.floor(seconds/3600),minutes=Math.floor(seconds%3600/60),rest=seconds%60;return hours?`${hours} год ${String(minutes).padStart(2,'0')} хв`:`${minutes} хв ${String(rest).padStart(2,'0')} с`;}
 function date(at){return at?new Date(at*1000).toLocaleString('uk-UA'):'—';}
 function validHistory(report){return report&&Number.isSafeInteger(report.window_seconds)&&report.window_seconds>=0&&Number.isSafeInteger(report.observations)&&report.observations>=0&&Array.isArray(report.rows)&&report.rows.length===27&&report.rows.every((row,i)=>row.index===i&&Object.keys(labels).concat('starts').every(key=>Number.isSafeInteger(row[key])&&row[key]>=0)&&Object.keys(labels).reduce((sum,key)=>sum+row[key],0)===report.window_seconds);}
 function renderHistory(){
  const renderKey=[state.historyGeneratedAt,state.selected,state.historyLoading,state.historyDays,state.historyError].join('|');if(renderKey===historyRenderKey)return;historyRenderKey=renderKey;
  $('history-load').disabled=!!state.historyLoading;$('history-period').disabled=!!state.historyLoading;
  $('history-period').value=String(state.historyDays??7);
  const report=validHistory(state.history)?state.history:null,hasData=report&&report.observations>0;
  $('history-content').hidden=!report;
  const message=state.historyLoading?'Розраховуємо локальну історію…':state.historyError||(state.history&&!report?'Звіт має некоректний формат. Спробуйте перерахувати історію.':!hasData?'За цей період ще немає спостережень. Залиште програму відкритою для накопичення історії.':report.window_seconds===0?'Потрібно щонайменше два спостереження для розрахунку тривалості.':'');
  text('history-message',message);$('history-message').hidden=!message;$('history-message').classList.toggle('error',!!state.historyError);
  document.querySelectorAll('[data-export]').forEach(button=>button.disabled=!report||!!state.historyLoading||!!state.historyError);
  if(!report)return;
  const chosen=report.rows[state.selected],total=report.window_seconds;
  text('history-samples',report.observations);text('history-window',duration(total));text('history-coverage',total?((total-chosen.unknown)/total*100).toFixed(1)+'%':'—');
  text('history-region-title',regions[state.selected].name);text('history-range',date(report.first)+' — '+date(report.last));
  $('history-bar').replaceChildren();$('history-durations').replaceChildren();
  for(const [key,[name,color]] of Object.entries(labels)){
   const segment=document.createElement('span');segment.className=color;segment.style.flexGrow=String(chosen[key]);segment.style.flexBasis='0';segment.title=name+': '+duration(chosen[key]);$('history-bar').append(segment);
   const label=document.createElement('span');label.textContent=name+' · ';const value=document.createElement('strong');value.textContent=duration(chosen[key]);label.append(value);$('history-durations').append(label);
  }
  const rows=report.rows.map((row,i)=>{const tr=document.createElement('tr');tr.classList.toggle('selected',i===state.selected);const name=document.createElement('th');name.scope='row';const select=document.createElement('button');select.textContent=regions[i].name;select.onclick=()=>chooseRegion(i);name.append(select);tr.append(name);for(const key of ['red','partial','threat','clear','unknown','starts']){const td=document.createElement('td');td.textContent=key==='starts'?row[key]:duration(row[key]);tr.append(td);}return tr;});
  $('history-rows').replaceChildren(...rows);
 }
 function renderStatus(){
  const connected=!!window.chrome?.webview,age=Date.now()/1000-(state.at||0),fresh=state.demo||(state.available&&age>=-5&&age<90);
  $('native-dot').classList.toggle('connected',connected);text('native-state',connected?'Програму підключено':'Перегляд інтерфейсу');
  let connection=state.demo?'Вигадані дані':state.loading?'Завантаження…':!state.keyConfigured?'Додайте API-ключ':state.authPaused?'Перевірте підключення':fresh?'Дані актуальні':state.retrySeconds?`Повтор через ${state.retrySeconds} с`:'Очікування даних';
  text('connection',connection);$('connection').className='connection '+(fresh&&!state.demo?'live':'waiting');$('mode').hidden=!state.demo;
  text('connection-detail',state.connectionText||'Очікування зв’язку з програмою…');$('demo-controls').hidden=!state.demo;
  const levels=MapModel.levels(state.levels,!!fresh),nativeCounts=[state.activeCount,state.clearCount,state.unknownCount];
  const useNative=fresh&&nativeCounts.every(Number.isInteger)&&nativeCounts.reduce((a,b)=>a+b,0)===27;
  text('active-number',useNative?state.activeCount:[...levels].filter(level=>'RPY'.includes(level)).length);text('clear-number',useNative?state.clearCount:[...levels].filter(level=>level==='N').length);text('unknown-number',useNative?state.unknownCount:[...levels].filter(level=>level==='?').length);
  $('refresh-api').disabled=!state.refreshAllowed;text('refresh-api',state.demo?'Деморежим':state.loading?'Завантаження…':!state.keyConfigured?'Додайте ключ у налаштуваннях':state.authPaused?'Підключення призупинено':state.retrySeconds?`Оновлення через ${state.retrySeconds} с`:'Оновити дані');
  $('live-mode').checked=!state.demo;$('forget-key').disabled=!state.keyConfigured&&!state.keySaved;
  text('key-state',state.keySaved?'Ключ збережено у Windows':state.keyConfigured?'Ключ активний лише в цьому сеансі':'Ключ ще не підключено');
  if(state.historyOk===false){text('native-state','Історія не записується');$('native-state').title='Перевірте вільне місце та доступ до папки даних.';}
 }
 function update(value){
  if(!value||value.version!==1||!Number.isInteger(value.selected)||value.selected<0||value.selected>=27)return;
  const revision=value.settingsRevision??0,changed=lastRevision!==revision;
  state=value;mapUpdate(state);syncRegion();
  if(!(pendingSave&&changed&&state.settingsError)&&(!initialized||(changed&&!settingsDirty)||pendingSave&&changed)){$('provider').value=state.provider;$('remember-key').checked=!!state.remember;settingsDirty=false;}
  if(changed){if(pendingSave){pendingSave=false;$('save-settings').disabled=false;settingsDirty=!!state.settingsError;}formMessage(state.settingsMessage||'',!!state.settingsError);if(initialized&&state.settingsMessage&&page!=='settings')toast(state.settingsMessage,!!state.settingsError);lastRevision=revision;}
  initialized=true;renderStatus();renderHistory();
 }
 window.airAlert.update=update;
 if(window.chrome?.webview){window.chrome.webview.addEventListener('message',event=>update(event.data));send('ready');}
 syncRegion();renderStatus();renderHistory();setInterval(renderStatus,1000);
})();
