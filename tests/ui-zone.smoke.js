async (page) => {
 let checks=0;const check=(ok,name)=>{if(!ok)throw Error(name);checks++;};
 await page.evaluate(()=>{
  window.chrome.webview={postMessage:()=>{}};
  window.zoneState={version:1,selected:4,provider:'ukrainealarm',available:true,demo:false,at:Date.now()/1000,levels:'N'.repeat(4)+'P'+'N'.repeat(22),territories:[{id:'14',name:'Київська область',type:'State',parent:'',oblast:4,level:'P',severityKnown:false}],keyConfigured:true,preferencesRevision:0,preferences:PreferencesModel.defaults()};
  airAlert.update(zoneState);
 });
 await page.locator('#territory-select').selectOption('Community:UA3200000');
 const zone=page.locator('#communities path[data-territory="Community:UA3200000"]');
 check(await zone.getAttribute('data-level')==='P','zone exposes partial oblast context');
 check(await zone.getAttribute('data-context')==='oblast','context has separate geographic marker');
 check((await page.locator('#territory-status').textContent()).includes('Київська область'),'status names the source oblast');
 check((await page.locator('#territory-source').textContent()).includes('не є окремим статусом зони'),'detail explains lack of independent zone evidence');
 check(await zone.evaluate(el=>getComputedStyle(el).fill.includes('oblast-context-fill')),'partial context uses dedicated gray-blue hatching');
 await page.evaluate(()=>airAlert.update({...zoneState,at:Date.now()/1000+1,levels:'R'.repeat(27)}));
 check(await zone.getAttribute('data-level')==='R'&&await zone.getAttribute('data-context')==='','whole-oblast alarm uses confirmed full coverage');
 await page.evaluate(()=>airAlert.update({...zoneState,at:Date.now()/1000-100}));
 check(await zone.getAttribute('data-level')==='?','stale context is never current');
 return 'PASS: '+checks+' Chernobyl context browser checks';
}
