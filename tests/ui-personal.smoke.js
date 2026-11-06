async (page) => {
 let checks=0;const check=(v,n)=>{if(!v)throw Error(n);checks++;};
 await page.setViewportSize({width:1280,height:950});
 await page.evaluate(()=>{
  window.testPackets=[];window.chrome.webview={postMessage:p=>testPackets.push(JSON.parse(p))};
  const g=AIRALERT_TERRITORIES,c=g.communities.find(s=>s.oblast===7&&s.apiId),d=g.districts.find(s=>s.oblast===7&&s.apiId),st=g.states.find(s=>s.oblast===7);
  window.personalShapes={c,d,st};window.personalKey='Community:'+c.code;
  const territories=[...g.states,...g.districts,...g.communities].filter(s=>s.apiId).map(s=>({id:s.apiId,name:s.apiName,parent:s.apiParent,oblast:s.oblast,type:s.kind,level:s===c?'R':'N',severityKnown:true,source:''}));
  const preferences={...PreferencesModel.defaults(),favorites:[personalKey,'State:'+st.code],home:personalKey,selectedTerritory:personalKey,theme:'graphite',brightness:90,borderWidth:120,view:{x:100,y:70,w:500,h:350}};
  window.personalState={version:1,selected:7,provider:'ukrainealarm',available:true,demo:false,at:Date.now()/1000,levels:'N'.repeat(7)+'P'+'N'.repeat(19),territories,keyConfigured:true,remember:true,settingsRevision:0,preferencesRevision:3,preferences,historyDays:7,pythonAvailable:true,appVersion:'0.4.0'};
  airAlert.update(personalState);
 });
 check(await page.locator('#favorites-list .favorite-card').count()===2,'restores favorite geometry keys');
 check((await page.locator('#favorites-list').textContent()).includes('червоний'),'favorites expose fresh actual status');
 check(await page.locator('#territory-select').inputValue()===await page.evaluate(()=>personalKey),'restores selected community');
 check(await page.locator('#map').getAttribute('viewBox')==='100 70 500 350','restores viewport');
 check(await page.locator('html').getAttribute('data-theme')==='graphite','restores muted alternate theme');
 await page.locator('#region-select').selectOption('25');
 await page.evaluate(()=>airAlert.update({...personalState,preferencesRevision:2}));
 check(await page.locator('#region-select').inputValue()==='25','older heartbeat cannot rewind current selection');
 await page.locator('#go-home').click();
 check(await page.locator('#territory-select').inputValue()===await page.evaluate(()=>personalKey),'home selects exact community');
 await page.locator('#favorite-toggle').click();
 check(await page.locator('#favorites-list .favorite-card').count()===1&&await page.locator('#go-home').isDisabled(),'removing home clears home');
 await page.evaluate(()=>{
  const latest=testPackets.filter(p=>p.type==='preferences').at(-1);
  if(latest)airAlert.update({...personalState,preferencesRevision:4,preferences:latest.preferences});
 });
 await page.evaluate(()=>airAlert.update({...personalState,at:Date.now()/1000-100}));
 check((await page.locator('#favorites-list').textContent()).includes('Немає підтверджених даних'),'stale favorites become unknown');
 await page.getByRole('button',{name:'Налаштування',exact:true}).click();
 await page.locator('#map-theme').selectOption('midnight');
 await page.evaluate(()=>airAlert.update({...personalState,preferencesRevision:3}));
 check(await page.locator('#map-theme').inputValue()==='midnight','heartbeat does not overwrite appearance draft');
 check((await page.locator('#about-version').textContent()).includes('0.4.0'),'about exposes native app version');
 await page.getByRole('button',{name:'Історія',exact:true}).click();
 const child=await page.evaluate(()=>personalShapes.c.apiId);
 await page.locator('#history-scope').selectOption(child);
 check(await page.evaluate(()=>testPackets.at(-1).type==='history'&&testPackets.at(-1).territory===personalShapes.c.apiId),'history sends numeric provider ID');
 await page.evaluate(()=>{
  const totals={red:60,partial:0,threat:0,clear:30,unknown:30,starts:1};
  const rows=Array.from({length:27},(_,index)=>({index,red:0,partial:0,threat:0,clear:120,unknown:0,starts:0}));
  airAlert.update({...personalState,historyGeneratedAt:1,historyTerritory:personalShapes.c.apiId,history:{rows,window_seconds:120,observations:3,first:100,last:220,territory:{id:personalShapes.c.apiId,name:personalShapes.c.name,type:'Community',oblast:7,...totals,daily:[{day:'2026-11-06',...totals}],events:[{at:160,previous:'N',level:'R'}]}}});
 });
 check(await page.locator('#history-territory-card').isVisible(),'selected history card visible');
 check(await page.locator('#history-daily [role=listitem]').count()===1,'selected history daily chart populated');
 check(await page.locator('#history-events li').count()===1,'confirmed transitions shown');
 check((await page.locator('#history-territory-coverage').textContent()).includes('75.0%'),'selected totals use report window');
 check(await page.locator('#history-rows tr').count()===27,'oblast table retained');
 await page.evaluate(()=>airAlert.update({...personalState,historyGeneratedAt:2,historyTerritory:'',history:{rows:Array.from({length:27},(_,index)=>({index,red:0,partial:0,threat:0,clear:120,unknown:0,starts:0})),window_seconds:120,observations:3,first:100,last:220,territory:null}}));
 check(!await page.locator('#history-territory-card').isVisible(),'old oblast report cannot pretend to be child history');
 await page.evaluate(()=>{const totals={red:0,partial:0,threat:0,clear:0,unknown:120,starts:0};airAlert.update({...personalState,historyGeneratedAt:3,historyTerritory:personalShapes.c.apiId,history:{rows:Array.from({length:27},(_,index)=>({index,...totals})),window_seconds:120,observations:3,first:100,last:220,territory:{id:personalShapes.c.apiId,name:personalShapes.c.apiId,type:'Unknown',oblast:-1,...totals,daily:[{day:'2026-11-06',...totals}],events:[]}}});});
 check(await page.locator('#history-territory-card').isVisible(),'old records without catalogs display valid unknown child history');
 check((await page.locator('#history-territory-coverage').textContent()).includes('0.0%')&&!await page.locator('[data-export=json]').isDisabled(),'all-unknown historical scope remains exportable');
 check(await page.evaluate(()=>localStorage.length===0&&sessionStorage.length===0),'personal keys never enter browser storage');
 return 'PASS: '+checks+' personal-territory browser checks';
}
