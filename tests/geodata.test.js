const assert=require('node:assert/strict'),test=require('node:test'),fs=require('node:fs'),vm=require('node:vm');
const scope={window:{}};vm.runInNewContext(fs.readFileSync('web/territories.js','utf8'),scope);
const data=scope.window.AIRALERT_TERRITORIES,mapping=require('../data/territory-mapping.json');
test('country, modern district and community geometry retain complete source coverage',()=>{
 assert.equal(data.states.length,27);assert.equal(data.districts.length,139);assert.equal(data.communities.length,1769);
 assert.equal(new Set(data.states.map(s=>s.oblast)).size,27);
 for(const f of [...data.states,...data.districts,...data.communities]){
  assert.ok(f.oblast>=0&&f.oblast<27&&f.rings.length>0);
  for(const ring of f.rings){assert.ok(ring.length>=4);assert.deepEqual(ring[0],ring.at(-1));for(const p of ring)assert.ok(p.every(Number.isFinite));}
 }
});
test('reviewed joins are one-to-one with exact provider IDs, parents and names',()=>{
 const shapes=[...data.districts,...data.communities].filter(s=>s.apiId);
 assert.equal(shapes.length,1579);assert.equal(new Set(shapes.map(s=>s.apiId)).size,1579);
 for(const s of shapes){const m=mapping.entries.find(e=>e.id===s.apiId);assert.ok(m);assert.equal(s.code,m.pcode);assert.equal(s.apiName,m.name);assert.equal(s.apiParent,m.parent);}
 assert.equal(shapes.filter(s=>s.kind==='District').length,122);
 assert.equal(shapes.filter(s=>s.kind==='Community').length,1457);
});
test('same-name city and rural communities have reviewed distinct geometry',()=>{
 for(const [id,code] of [['354','UA1208009'],['353','UA1208011'],['969','UA5110037'],['970','UA5110039']])
  assert.equal(data.communities.find(s=>s.apiId===id).code,code);
 assert.equal(data.communities.find(s=>s.apiId==='1180').code,'UA5908015');
 assert.equal(mapping.unresolved.length,0);
});
test('separate city State roots bind to their actual city community geometry',()=>{
 for(const [id,code,parent] of [['564','UA2306007','12'],['1293','UA6312027','22']]){
  const s=data.communities.find(s=>s.apiId===id);assert.equal(s.code,code);assert.equal(s.apiParent,parent);
 }
});
