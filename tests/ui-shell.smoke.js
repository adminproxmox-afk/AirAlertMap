async (page) => {
 let checks=0; const check=(value,name)=>{if(!value)throw new Error(name);checks++;};
 await page.setViewportSize({width:1280,height:850});
 await page.evaluate(()=>{
  window.testPackets=[];
  window.chrome.webview={postMessage:message=>window.testPackets.push(JSON.parse(message))};
  window.testState={version:1,levels:'N'.repeat(7)+'P'+'N'.repeat(19),selected:7,provider:'ukrainealarm',available:true,demo:false,at:Date.now()/1000,activeCount:1,clearCount:26,unknownCount:0,connectionText:'Browser test fixture',keyConfigured:true,keySaved:true,remember:true,settingsRevision:0,historyDays:7,historyGeneratedAt:0,historyLoading:false,refreshAllowed:true};
  window.airAlert.update(window.testState);
 });
 check(await page.locator('#active-number').textContent()==='1','native ASM count is shown');
 await page.getByRole('button',{name:'Налаштування',exact:true}).click();
 check(await page.locator('#page-settings').isVisible()&&!await page.locator('#page-map').isVisible(),'settings opens in same page');
 check(await page.locator('#api-token').inputValue()==='','saved key is never echoed');
 await page.locator('#provider').selectOption('alerts.in.ua');
 await page.evaluate(()=>window.airAlert.update({...window.testState,retrySeconds:12}));
 check(await page.locator('#provider').inputValue()==='alerts.in.ua','heartbeat preserves unsaved provider');
 await page.locator('#api-token').fill('unit-test-key');
 await page.locator('#remember-key').uncheck();
 await page.locator('#save-settings').click();
 check(await page.locator('#api-token').inputValue()==='','submitted key cleared');
 check(await page.evaluate(()=>{const p=window.testPackets.at(-1);return p.type==='saveSettings'&&p.version===1&&p.token==='unit-test-key'&&p.provider==='alerts.in.ua'&&!p.remember;}),'settings packet matches native protocol');
 await page.evaluate(()=>window.airAlert.update({...window.testState,settingsRevision:1,settingsError:true,settingsMessage:'Synthetic storage failure'}));
 check(await page.locator('#provider').inputValue()==='alerts.in.ua'&&!await page.locator('#remember-key').isChecked(),'failed save preserves retry choices');
 await page.getByRole('button',{name:'Історія',exact:true}).click();
 check(await page.evaluate(()=>window.testPackets.at(-1).type==='history'),'history requests Python data');
 await page.evaluate(()=>{
  const rows=Array.from({length:27},(_,index)=>({index,red:index===7?60:0,partial:0,threat:0,clear:index===7?60:120,unknown:0,starts:0}));
  window.airAlert.update({...window.testState,historyGeneratedAt:1,history:{rows,window_seconds:120,observations:3,rejected:0,first:100,last:220}});
 });
 check(await page.locator('#history-rows tr').count()===27,'all regions shown in inline history');
 check(await page.locator('#history-coverage').textContent()==='100.0%','history coverage uses selected region');
 await page.locator('[data-export=csv]').click();
 check(await page.evaluate(()=>{const p=window.testPackets.at(-1);return p.type==='export'&&p.format==='csv'&&Object.keys(p).length===3;}),'export sends only an allowed format');
 await page.locator('#history-region').selectOption('25');
 check(await page.evaluate(()=>window.testPackets.findLast(p=>p.type==='select')?.region===25&&window.testPackets.at(-1).type==='history'),'selection is sent to C++');
 await page.getByRole('button',{name:'Мапа',exact:true}).click();
 check(await page.locator('#region-select').inputValue()==='25','selected region is shared between pages');
 await page.evaluate(()=>window.airAlert.update({...window.testState,selected:25,at:Date.now()/1000-100}));
 check(await page.locator('#unknown-number').textContent()==='27','stale states become unknown');
 check(await page.evaluate(()=>localStorage.length===0&&sessionStorage.length===0),'no API key is persisted in browser storage');
 await page.setViewportSize({width:900,height:700});
 check(await page.getByRole('button',{name:'Налаштування',exact:true}).count()===1,'compact navigation keeps accessible labels');
 check(await page.evaluate(()=>document.body.scrollWidth<=innerWidth),'compact layout stays within window');
 await page.setViewportSize({width:1280,height:850});
 return 'PASS: '+checks+' single-window browser checks';
}
