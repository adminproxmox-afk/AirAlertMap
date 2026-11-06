async (page) => {
 let checks=0;const check=(v,n)=>{if(!v)throw Error(n);checks++;};
 await page.setViewportSize({width:1280,height:950});
 await page.evaluate(()=>{
  window.testPackets=[];window.chrome.webview={postMessage:p=>testPackets.push(JSON.parse(p))};
  const shape=AIRALERT_TERRITORIES.states.find(s=>s.oblast===7),key='State:'+shape.code;
  window.prefState={version:1,selected:7,provider:'ukrainealarm',at:Date.now()/1000,available:true,demo:false,levels:'N'.repeat(27),keyConfigured:true,preferencesRevision:0,preferences:{...PreferencesModel.defaults(),favorites:[key],home:key,selectedTerritory:key}};
  airAlert.update(prefState);
 });
 await page.getByRole('button',{name:'Налаштування',exact:true}).click();
 await page.evaluate(()=>{const range=document.getElementById('map-brightness');range.value='80';range.dispatchEvent(new Event('input',{bubbles:true}));airAlert.update(prefState);range.value='75';range.dispatchEvent(new Event('input',{bubbles:true}));});
 check(await page.evaluate(()=>testPackets.filter(p=>p.type==='preferences').length===0),'heartbeat does not flush a gesture before debounce');
 await page.waitForTimeout(420);
 check(await page.evaluate(()=>{const p=testPackets.filter(p=>p.type==='preferences');return p.length===1&&p[0].preferences.brightness===75&&p[0].preferences.home.startsWith('State:UA')&&p[0].version===1;}),'debounced save sends latest full preference payload');
 await page.locator('#map-theme').selectOption('midnight');
 check(await page.evaluate(()=>testPackets.filter(p=>p.type==='preferences').length===1),'inflight save queues next settings change');
 await page.evaluate(()=>{const first=testPackets.find(p=>p.type==='preferences').preferences;airAlert.update({...prefState,preferencesRevision:1,preferences:Object.fromEntries(Object.entries(first).reverse())});});
 check(await page.evaluate(()=>{const packets=testPackets.filter(p=>p.type==='preferences');return packets.length===2&&packets[1].preferences.theme==='midnight'&&packets[1].preferences.brightness===75;}),'acknowledgement flushes current draft with reordered native fields');
 await page.evaluate(()=>airAlert.update({...prefState,preferencesRevision:2,preferencesError:'Synthetic write failure'}));
 check(await page.locator('#map-theme').inputValue()==='midnight'&&await page.locator('#map-brightness').inputValue()==='75','failed save preserves draft controls');
 check((await page.locator('#action-notice').textContent()).includes('Synthetic write failure'),'failed native persistence is visible');
 await page.locator('#map-theme').selectOption('graphite');
 await page.evaluate(()=>{const p=testPackets.filter(p=>p.type==='preferences').at(-1).preferences;window.savedPref=p;airAlert.update({...prefState,preferencesRevision:3,preferences:p});});
 await page.getByRole('button',{name:'Мапа',exact:true}).click();
 await page.locator('#focus-region').click();
 await page.evaluate(()=>{const p=testPackets.filter(p=>p.type==='preferences').at(-1).preferences;airAlert.update({...prefState,preferencesRevision:4,preferences:p});});
 const rect=await page.locator('#map').boundingBox();
 await page.mouse.move(rect.x+rect.width*.55,rect.y+rect.height*.5);await page.mouse.down();await page.mouse.move(rect.x+rect.width*.65,rect.y+rect.height*.55,{steps:5});await page.mouse.up();
 check(await page.evaluate(()=>{const p=testPackets.filter(p=>p.type==='preferences').at(-1).preferences;const v=document.getElementById('map').getAttribute('viewBox').split(' ').map(Number);return [p.view.x,p.view.y,p.view.w,p.view.h].every((n,i)=>Math.abs(n-v[i])<1e-8);}), 'pointerup immediately saves the final panned viewport');
 check(await page.evaluate(()=>getSelection().toString()===''),'drag does not select text');
 const restored=await page.evaluate(()=>({...prefState,preferencesRevision:5,preferences:testPackets.filter(p=>p.type==='preferences').at(-1).preferences}));
 await page.reload();
 await page.evaluate(value=>{window.testPackets=[];window.chrome.webview={postMessage:p=>testPackets.push(JSON.parse(p))};airAlert.update(value);},restored);
 check(await page.locator('#map').getAttribute('viewBox')===[restored.preferences.view.x,restored.preferences.view.y,restored.preferences.view.w,restored.preferences.view.h].join(' '),'native saved viewport restores after reload');
 check(await page.locator('#favorites-list .favorite-card').count()===1&&await page.locator('html').getAttribute('data-theme')==='graphite','native favorites and theme restore after reload');
 check(await page.evaluate(()=>localStorage.length===0&&sessionStorage.length===0),'reload uses no browser storage');
 return 'PASS: '+checks+' preference lifecycle browser checks';
}
