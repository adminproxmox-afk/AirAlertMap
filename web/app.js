'use strict';
(() => {
 const $=id=>document.getElementById(id), regions=window.AIRALERT_REGIONS;
 const mapUpdate=window.airAlert.update.bind(window.airAlert);
 const titles={map:['Мапа тривог','ПОТОЧНА ОБСТАНОВКА'],history:['Історія спостережень','ЛОКАЛЬНА АНАЛІТИКА'],settings:['Налаштування','ВАШЕ ПІДКЛЮЧЕННЯ']};
 const labels={red:['Повна тривога','red'],partial:['Часткова','amber'],threat:['Попередня загроза','amber'],clear:['Без тривоги','clear'],unknown:['Немає даних','unknown']};
 const preferenceController=PreferencesModel.create();let preferenceTimer;
 let historyScope='',historyOptionRegion=-1,historyUnmapped=false;
 let retentionDirty=false,pendingStorage=null,confirmationDays=0,pendingDiagnostics=null;
 const catalog=[...(window.AIRALERT_TERRITORIES?.districts||[]),...(window.AIRALERT_TERRITORIES?.communities||[])];
 let state={version:1,levels:'?'.repeat(27),selected:7,provider:'ukrainealarm',available:false,demo:false,keyConfigured:false,keySaved:false,remember:true,settingsRevision:0,historyDays:7},page='map',initialized=false,settingsDirty=false,pendingSave=false,lastRevision=-1,historyRenderKey='',toastTimer;
 function text(id,value){const element=$(id),next=String(value??'');if(element.textContent!==next)element.textContent=next;}
 function toast(message,error=false){text('action-notice',message);$('action-notice').classList.toggle('error',error);$('action-notice').hidden=false;clearTimeout(toastTimer);toastTimer=setTimeout(()=>$('action-notice').hidden=true,6500);}
 function send(type,payload={}) {
  if(!window.chrome?.webview?.postMessage){toast('Відкрийте AirAlertMap.exe, щоб застосувати цю дію.',true);return false;}
  try{window.chrome.webview.postMessage(JSON.stringify({...payload,version:1,type}));return true;}
  catch{toast('Зв’язок із програмою недоступний. Перезапустіть застосунок.',true);return false;}
 }
 function flushPreferences(){clearTimeout(preferenceTimer);preferenceTimer=undefined;const value=preferenceController.prepare();if(value&&!send('preferences',{preferences:value}))preferenceController.cancel();}
 function applyAppearance(value){document.documentElement.dataset.theme=value.theme;document.documentElement.style.setProperty('--map-brightness',value.brightness/100);document.documentElement.style.setProperty('--border-scale',value.borderWidth/100);$('map-theme').value=value.theme;$('map-brightness').value=value.brightness;$('map-border-width').value=value.borderWidth;text('brightness-value',value.brightness+'%');text('border-width-value',value.borderWidth+'%');}
 function changePreferences(patch,immediate=true){if(!preferenceController.set(patch))return;const value=preferenceController.value();window.airAlert.setPreferences(value);applyAppearance(value);text('preferences-message','');if(immediate)flushPreferences();else{clearTimeout(preferenceTimer);preferenceTimer=setTimeout(flushPreferences,350);}}
 function requestHistory(){const payload={days:Number($('history-period').value)};if(historyScope)payload.territory=historyScope;send('history',payload);}
 function chooseRegion(index){if(!Number.isInteger(index)||index<0||index>=27)return;state={...state,selected:index};window.airAlert.selectRegion?.(index);mapUpdate(state);syncRegion();historyRenderKey='';renderHistory();send('select',{region:index});}
 window.appBridge={select:chooseRegion,preferences:changePreferences};
 $('map-theme').onchange=e=>changePreferences({theme:e.target.value});
 for(const [id,key] of [['map-brightness','brightness'],['map-border-width','borderWidth']]){const control=$(id);control.oninput=()=>changePreferences({[key]:Number(control.value)},false);control.onchange=()=>changePreferences({[key]:Number(control.value)});control.addEventListener('pointerup',flushPreferences);}

 for(const id of ['history-region','settings-region'])regions.forEach(region=>{const option=document.createElement('option');option.value=region.index;option.textContent=region.name;$(id).append(option);});
 function syncRegion(){for(const id of ['history-region','settings-region'])$(id).value=String(state.selected);
  if(historyOptionRegion!==state.selected){historyOptionRegion=state.selected;historyScope='';historyUnmapped=false;const option=document.createElement('option');option.value='';option.textContent='Уся область';const options=catalog.filter(s=>s.oblast===state.selected&&/^[0-9]{1,10}$/.test(s.apiId)).sort((a,b)=>a.kind.localeCompare(b.kind)||a.name.localeCompare(b.name,'uk')).map(shape=>{const o=document.createElement('option');o.value=shape.apiId;o.textContent=(shape.kind==='District'?'Район · ':'Громада · ')+shape.name;return o;});$('history-scope').replaceChildren(option,...options);}
  $('history-scope').value=historyScope;
 }
 function navigate(next){if(!titles[next])return;page=next;for(const name of Object.keys(titles))$('page-'+name).hidden=name!==next;document.querySelectorAll('[data-page]').forEach(button=>{const active=button.dataset.page===next;button.classList.toggle('active',active);if(active)button.setAttribute('aria-current','page');else button.removeAttribute('aria-current');});text('page-title',titles[next][0]);text('section-caption',titles[next][1]);document.title=titles[next][0]+' · AirAlertMap';text('footer-caption',next==='map'?'Межі: Kartographia / OCHA · CC BY 3.0 IGO':'AirAlertMap · Локальні спостереження');if(next==='history'){renderHistory();if(!state.historyLoading)requestHistory();}}
 document.querySelectorAll('[data-page]').forEach(button=>button.addEventListener('click',()=>navigate(button.dataset.page)));
 $('region-history').onclick=()=>{const key=window.airAlert.selectedTerritory(),shape=catalog.find(s=>s.kind+':'+s.code===key);syncRegion();historyScope=shape?.apiId||'';historyUnmapped=!!shape&&!shape.apiId;$('history-scope').value=historyScope;historyRenderKey='';navigate('history');};
 $('history-scope').onchange=e=>{historyScope=e.target.value;historyUnmapped=false;historyRenderKey='';renderHistory();requestHistory();};
 $('history-region').onchange=e=>{chooseRegion(Number(e.target.value));requestHistory();};$('settings-region').onchange=e=>chooseRegion(Number(e.target.value));
 $('refresh-api').onclick=()=>send('refresh');
 document.querySelectorAll('[data-command]').forEach(button=>button.onclick=()=>send(button.dataset.command));
 $('live-mode').onchange=e=>{if(!send('mode',{live:e.target.checked}))e.target.checked=!state.demo;};
 $('history-period').onchange=()=>requestHistory();
 $('history-load').onclick=()=>requestHistory();
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
 const retentionOptions=[0,90,180,365];
 function storageValue(){return state.historyStorage||{};}
 function maintenanceUnavailable(){return !!pendingStorage||!!storageValue().busy||!!state.historyLoading||state.pythonAvailable!==true;}
 function storageCommand(type,payload={}){if(pendingStorage||(type!=='storageStats'&&maintenanceUnavailable()))return;if(send(type,payload)){pendingStorage={revision:storageValue().revision,type,days:payload.days};renderStorage();}}
 $('refresh-storage').onclick=()=>storageCommand('storageStats');
 $('archive-history').onclick=()=>storageCommand('archiveHistory');
 $('history-retention').onchange=()=>retentionDirty=true;
 $('apply-retention').onclick=()=>{
  if(maintenanceUnavailable())return;const days=Number($('history-retention').value);if(!retentionOptions.includes(days))return;
  if(!days){storageCommand('retention',{days:0,confirm:true});return;}
  confirmationDays=days;text('retention-description',`Спостереження, старші за ${days} днів, буде видалено зараз. Програма збереже цей термін і автоматично очищатиме давні спостереження під час наступних запусків.`);$('retention-dialog').showModal();
 };
 $('cancel-retention').onclick=()=>$('retention-dialog').close();
 $('confirm-retention').onclick=()=>{if(maintenanceUnavailable()||!retentionOptions.includes(confirmationDays)||!confirmationDays)return;$('retention-dialog').close();storageCommand('retention',{days:confirmationDays,confirm:true});};
 $('copy-diagnostics').onclick=()=>{if(pendingDiagnostics!==null||typeof state.diagnostics!=='string'||!state.diagnostics)return;if(send('copyDiagnostics')){pendingDiagnostics=state.diagnosticsRevision??0;renderStorage();}};
 function byteSize(value){if(!Number.isFinite(value)||value<0)return '—';if(value<1024)return Math.floor(value)+' Б';const units=['КіБ','МіБ','ГіБ','ТіБ'];let unit=-1;do{value/=1024;unit++;}while(value>=1024&&unit<units.length-1);return value.toLocaleString('uk-UA',{maximumFractionDigits:1})+' '+units[unit];}
 function renderStorage(){
  const storage=storageValue();
  if(pendingStorage&&Number.isInteger(storage.revision)&&storage.revision!==pendingStorage.revision){if(pendingStorage.type==='retention'&&!storage.error&&storage.retentionDays===pendingStorage.days)retentionDirty=false;pendingStorage=null;}
  if(!retentionDirty&&retentionOptions.includes(storage.retentionDays))$('history-retention').value=String(storage.retentionDays);
  const files=Number.isSafeInteger(storage.files)&&storage.files>=0?storage.files:'—',catalogs=Number.isSafeInteger(storage.catalogs)&&storage.catalogs>=0?storage.catalogs:'—';
  text('storage-summary',Number.isFinite(storage.bytes)?`${byteSize(storage.bytes)} · файлів: ${files}`:'Розмір ще не отримано');
  text('storage-catalogs',Number.isFinite(storage.catalogBytes)?`Зокрема довідники територій: ${byteSize(storage.catalogBytes)} · файлів: ${catalogs}`:'');
  const unavailable=maintenanceUnavailable();$('archive-history').disabled=unavailable;$('apply-retention').disabled=unavailable;$('confirm-retention').disabled=unavailable;
  $('refresh-storage').disabled=!!pendingStorage||!!storage.busy;$('history-retention').disabled=!!pendingStorage||!!storage.busy;
  $('storage-python').hidden=state.pythonAvailable===true;text('storage-python',state.pythonAvailable===false?'Python не знайдено. Для ZIP-архіву та зміни терміну потрібен Python 3. Розмір сховища можна оновити без Python.':'Перевірка Python…');
  text('storage-message',pendingStorage?'Очікування відповіді програми…':storage.message||(storage.busy?'Обробляємо сховище історії…':''));$('storage-message').classList.toggle('error',!pendingStorage&&!!storage.error);$('storage-message').setAttribute('aria-busy',String(!!pendingStorage||!!storage.busy));
  if(pendingDiagnostics!==null&&(state.diagnosticsRevision??0)!==pendingDiagnostics)pendingDiagnostics=null;
  text('diagnostics-preview',typeof state.diagnostics==='string'?state.diagnostics:'');
  $('copy-diagnostics').disabled=pendingDiagnostics!==null||!state.diagnostics;
  text('diagnostics-message',pendingDiagnostics!==null?'Копіювання діагностики…':state.diagnosticsMessage||'');$('diagnostics-message').classList.toggle('error',pendingDiagnostics===null&&!!state.diagnosticsError);
 }
 function duration(seconds){seconds=Math.max(0,Math.floor(Number(seconds)||0));const hours=Math.floor(seconds/3600),minutes=Math.floor(seconds%3600/60),rest=seconds%60;return hours?`${hours} год ${String(minutes).padStart(2,'0')} хв`:`${minutes} хв ${String(rest).padStart(2,'0')} с`;}
 function date(at){return at?new Date(at*1000).toLocaleString('uk-UA'):'—';}
 function validHistory(report){return report&&Number.isSafeInteger(report.window_seconds)&&report.window_seconds>=0&&Number.isSafeInteger(report.observations)&&report.observations>=0&&Array.isArray(report.rows)&&report.rows.length===27&&report.rows.every((row,i)=>row.index===i&&Object.keys(labels).concat('starts').every(key=>Number.isSafeInteger(row[key])&&row[key]>=0)&&Object.keys(labels).reduce((sum,key)=>sum+row[key],0)===report.window_seconds);}
 function renderHistory(){
  const renderKey=[state.historyGeneratedAt,state.selected,state.historyLoading,state.historyDays,state.historyError,state.historyTerritory,historyScope,historyUnmapped,state.pythonAvailable].join('|');if(renderKey===historyRenderKey)return;historyRenderKey=renderKey;
  $('history-load').disabled=!!state.historyLoading;$('history-period').disabled=!!state.historyLoading;
  $('history-period').value=String(state.historyDays??7);
  const report=validHistory(state.history)?state.history:null,hasData=report&&report.observations>0;
  $('history-content').hidden=!report;
  const message=state.pythonAvailable===false?'Python не знайдено. Мапа працює; для історії потрібен Python 3. Встановіть його та перезапустіть застосунок.':state.historyLoading?'Розраховуємо локальну історію…':state.historyError||(state.history&&!report?'Звіт має некоректний формат. Спробуйте перерахувати історію.':!hasData?'За цей період ще немає спостережень. Залиште програму відкритою для накопичення історії.':report.window_seconds===0?'Потрібно щонайменше два спостереження для розрахунку тривалості.':'');
  text('history-message',message);$('history-message').hidden=!message;$('history-message').classList.toggle('error',!!state.historyError);
  document.querySelectorAll('[data-export]').forEach(button=>button.disabled=!report||!!state.historyLoading||!!state.historyError);
  renderTerritoryHistory(report);
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
 function validTerritory(territory,total){
  const durations=row=>row&&Object.keys(labels).concat('starts').every(k=>Number.isSafeInteger(row[k])&&row[k]>=0);
  return !!(territory&&typeof territory.id==='string'&&/^[0-9]{1,10}$/.test(territory.id)&&typeof territory.name==='string'&&['District','Community','State','Unknown'].includes(territory.type)&&durations(territory)&&Object.keys(labels).reduce((sum,k)=>sum+territory[k],0)===total&&Array.isArray(territory.daily)&&territory.daily.length<=36600&&territory.daily.every(d=>durations(d)&&/^\d{4}-\d{2}-\d{2}$/.test(d.day))&&Object.keys(labels).every(k=>territory.daily.reduce((sum,d)=>sum+d[k],0)===territory[k])&&Array.isArray(territory.events)&&territory.events.length<=200&&territory.events.every(e=>Number.isSafeInteger(e.at)&&e.at>0&&['N','R','Y','P','?'].includes(e.previous)&&['N','R','Y','P','?'].includes(e.level)));
 }
 function durationParts(row,bar,legend){bar.replaceChildren();if(legend)legend.replaceChildren();for(const [key,[name,color]] of Object.entries(labels)){const span=document.createElement('span');span.className=color;span.style.flexGrow=String(row[key]);span.style.flexBasis='0';span.title=name+': '+duration(row[key]);bar.append(span);if(legend){const label=document.createElement('span');label.textContent=name+' · '+duration(row[key]);legend.append(label);}}}
 function renderTerritoryHistory(report){
  const territory=report?.territory,matching=historyScope&&state.historyTerritory===historyScope&&territory?.id===historyScope,valid=matching&&validTerritory(territory,report.window_seconds);
  $('history-territory-card').hidden=!valid;let message=historyUnmapped?'Ця територія не має перевіреного ID постачальника, тому окрему історію показати неможливо. Нижче — історія області.':'';
  if(historyScope&&!valid)message=state.historyLoading?'Завантажуємо історію обраної території…':matching?'Детальний звіт має некоректний формат. Перерахуйте історію.':'Історія цієї території ще не завантажена. Натисніть «Перерахувати». Старі записи областей не містять даних громад і районів.';
  text('history-territory-message',message);$('history-territory-message').hidden=!message;
  document.querySelectorAll('[data-export]').forEach(button=>{if(historyScope&&!valid)button.disabled=true;});
  if(!valid)return;
  text('history-territory-title',territory.name+' · '+(territory.type==='Community'?'громада':territory.type==='District'?'район':territory.type==='State'?'область':'територія · немає історичних даних каталогу'));
  text('history-territory-coverage','Підтверджене покриття: '+(report.window_seconds?((report.window_seconds-territory.unknown)/report.window_seconds*100).toFixed(1)+'%':'—')+' · Початків тривоги: '+territory.starts);
  durationParts(territory,$('history-territory-bar'),$('history-territory-durations'));
  const days=territory.daily.map(day=>{const row=document.createElement('div');row.className='daily-row';row.setAttribute('role','listitem');const title=document.createElement('span');title.textContent=day.day;const bar=document.createElement('div');bar.className='duration-bar';durationParts(day,bar);bar.setAttribute('role','img');bar.setAttribute('aria-label',Object.entries(labels).map(([k,[name]])=>name+': '+duration(day[k])).join(', '));const total=document.createElement('span');total.className='muted';total.textContent='Тривога: '+duration(day.red+day.partial)+' · невідомо: '+duration(day.unknown);row.append(title,bar,total);return row;});$('history-daily').replaceChildren(...days);
  const levelName={N:'Без тривоги',R:'Червоний рівень',Y:'Попередня загроза / жовтий рівень',P:'Часткова тривога','?':'Немає даних'};
  $('history-events').replaceChildren(...territory.events.map(event=>{const li=document.createElement('li');li.textContent=date(event.at)+' · '+levelName[event.previous]+' → '+levelName[event.level];return li;}));$('history-events-empty').hidden=territory.events.length>0;
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
  text('about-version','AirAlertMap · '+(state.appVersion||'версію ще не отримано'));text('about-python',state.pythonAvailable===false?'Python не знайдено. Мапа доступна, історія потребує Python 3.':state.pythonAvailable===true?'Python доступний · історія обробляється локально.':'Перевірка Python…');
  text('key-state',state.keySaved?'Ключ збережено у Windows':state.keyConfigured?'Ключ активний лише в цьому сеансі':'Ключ ще не підключено');
  if(state.historyOk===false){text('native-state','Історія не записується');$('native-state').title='Перевірте вільне місце та доступ до папки даних.';}
 }
 function update(value){
  if(!value||value.version!==1||!Number.isInteger(value.selected)||value.selected<0||value.selected>=27)return;
  const revision=value.settingsRevision??0,changed=lastRevision!==revision;
  state=value;mapUpdate(state);const preferenceResult=preferenceController.receive(state);if(preferenceResult.apply){window.airAlert.setPreferences(preferenceResult.apply,true);applyAppearance(preferenceResult.apply);if(state.selected!==window.airAlert.selectedRegion())send('select',{region:window.airAlert.selectedRegion()});}if(preferenceResult.error){text('preferences-message','Не вдалося зберегти ваш перегляд: '+preferenceResult.error);toast('Налаштування перегляду не збережено: '+preferenceResult.error,true);}state={...state,selected:window.airAlert.selectedRegion()};syncRegion();if((preferenceResult.apply||preferenceResult.acknowledged)&&!preferenceTimer)flushPreferences();
  if(!(pendingSave&&changed&&state.settingsError)&&(!initialized||(changed&&!settingsDirty)||pendingSave&&changed)){$('provider').value=state.provider;$('remember-key').checked=!!state.remember;settingsDirty=false;}
  if(changed){if(pendingSave){pendingSave=false;$('save-settings').disabled=false;settingsDirty=!!state.settingsError;}formMessage(state.settingsMessage||'',!!state.settingsError);if(initialized&&state.settingsMessage&&page!=='settings')toast(state.settingsMessage,!!state.settingsError);lastRevision=revision;}
  initialized=true;renderStatus();renderHistory();renderStorage();
 }
 window.airAlert.update=update;
 if(window.chrome?.webview){window.chrome.webview.addEventListener('message',event=>update(event.data));send('ready');}
 syncRegion();renderStatus();renderHistory();renderStorage();setInterval(renderStatus,1000);
})();
