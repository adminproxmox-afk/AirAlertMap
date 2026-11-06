const assert=require('node:assert/strict'),test=require('node:test');
const model=require('../web/detail-model.js');
const shape={apiId:'A',apiName:'Town',apiParent:'D',oblast:7,kind:'Community'};
const row={id:'A',name:'Town',parent:'D',oblast:7,level:'Y',severityKnown:true};
test('exact reviewed ID, name and parent select a true yellow community',()=>{
 const s=model.status(shape,model.index([row]),'P'.repeat(27),true);
 assert.equal(s.level,'Y');assert.equal(s.precision,'Community');assert.match(model.text(s),/жовтий/);
});
test('changed names, parent and ambiguous IDs cannot bind a polygon',()=>{
 for(const t of [{...row,name:'Other'},{...row,parent:'Other'},{...row,oblast:8}])
  assert.equal(model.status(shape,model.index([t]),'P'.repeat(27),true).level,'?');
 assert.equal(model.index([row,row]).size,0);
});
test('partial coverage and true yellow are different states',()=>{
 assert.match(model.text({level:'P'}),/частині/);
 assert.doesNotMatch(model.text({level:'P'}),/жовтий/);
 assert.match(model.text({level:'R',severityKnown:false}),/не вказано/);
});
test('missing geometry binding only inherits confirmed whole oblast coverage',()=>{
 assert.equal(model.status(shape,new Map(),'R'.repeat(27),true).level,'R');
 assert.equal(model.status(shape,new Map(),'N'.repeat(27),true).precision,'State');
 assert.equal(model.status(shape,new Map(),'P'.repeat(27),true).level,'?');
 assert.equal(model.status(shape,model.index([row]),'N'.repeat(27),false).level,'?');
});
test('zoom reveals finer boundaries and users can override automatic detail',()=>{
 assert.equal(model.layer('auto',1,true),'State');
 assert.equal(model.layer('auto',2,true),'District');
 assert.equal(model.layer('auto',4,true),'Community');
 assert.equal(model.layer('Community',1,true),'Community');
 assert.equal(model.layer('Community',5,false),'State');
});
test('unlisted territories explain provider coverage instead of a connection failure',()=>{
 const records=new Map(),levels='P'.repeat(27);
 const zone={code:'UA3200000',oblast:4},city={oblast:26},ambiguous={code:'UA5908013',oblast:6};
 assert.match(model.explanation(model.status(zone,records,levels,true),zone),/не має окремого regionId/);
 assert.match(model.explanation(model.status(city,records,levels,true),city),/Севастополя немає/);
 assert.match(model.explanation(model.status(ambiguous,records,levels,true),ambiguous),/не надає окремого/);
 assert.equal(model.status(zone,records,levels,true).level,'?');
});
