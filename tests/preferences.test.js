'use strict';
const assert=require('node:assert/strict');
const test=require('node:test');
const model=require('../web/preferences.js');
test('preference model is available',()=>assert.equal(typeof model.create,'function'));
test('strict preferences protect geometry identities and bounded view',()=>{
 const p=model.defaults();assert.ok(model.valid(p));
 for(const patch of [{favorites:['State:7']},{favorites:['State:UA63','State:UA63']},{home:'State:UA63'},{brightness:59},{borderWidth:161},{view:{x:0,y:0,w:31,h:21.7}},{view:{x:800,y:0,w:500,h:350}}])assert.equal(model.valid({...p,...patch}),false);
 assert.equal(model.valid({...p,favorites:['State:UA63'],home:'State:UA63',view:{x:0,y:0,w:31.25,h:21.875}}),true);
});
test('first native preferences restore once and heartbeats cannot rewind local selection',()=>{
 const c=model.create(),initial={...model.defaults(),selectedTerritory:'Community:UA6312027',theme:'midnight'};
 assert.ok(c.receive({preferences:initial,preferencesRevision:4}).apply);
 c.set({selectedTerritory:'State:UA80'});
 assert.equal(c.receive({preferences:initial,preferencesRevision:4}).apply,null);
 assert.equal(c.value().selectedTerritory,'State:UA80');
});
test('one inflight save queues new edits and rejects stale acknowledgements',()=>{
 const c=model.create();c.receive({preferences:model.defaults(),preferencesRevision:2});
 c.set({theme:'graphite'});const first=c.prepare();assert.equal(first.theme,'graphite');
 c.set({brightness:80});assert.equal(c.prepare(),null);
 c.receive({preferences:model.defaults(),preferencesRevision:1});assert.equal(c.prepare(),null);
 c.receive({preferences:first,preferencesRevision:3});const second=c.prepare();assert.equal(second.theme,'graphite');assert.equal(second.brightness,80);
 c.receive({preferences:first,preferencesRevision:3});assert.equal(c.prepare(),null);
 c.receive({preferences:second,preferencesRevision:4});assert.equal(c.prepare(),null);
});
test('native save failure keeps the current draft available for retry',()=>{
 const c=model.create();c.receive({preferences:model.defaults(),preferencesRevision:0});c.set({brightness:75});c.prepare();
 const result=c.receive({preferences:model.defaults(),preferencesRevision:1,preferencesError:'disk full'});
 assert.equal(result.error,'disk full');assert.equal(c.value().brightness,75);assert.equal(c.prepare(),null);
 c.set({brightness:76});assert.equal(c.prepare().brightness,76);
});
test('local edits before the first native response survive restore',()=>{
 const c=model.create();c.set({brightness:85});c.receive({preferences:{...model.defaults(),theme:'graphite'},preferencesRevision:0});
 assert.equal(c.value().brightness,85);assert.equal(c.value().theme,'graphite');
});
test('native JSON property order and view rounding do not stall the save queue',()=>{
 const c=model.create();c.receive({preferences:model.defaults(),preferencesRevision:0});
 c.set({view:{x:1/3,y:1/3,w:500,h:350}});const sent=c.prepare();c.set({brightness:88});
 const reordered=Object.fromEntries(Object.entries(sent).reverse());reordered.view={h:350,w:500,y:.333333,x:.333333};
 c.receive({preferences:reordered,preferencesRevision:1});assert.equal(c.prepare()?.brightness,88);
});
