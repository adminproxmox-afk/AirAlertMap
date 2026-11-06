const assert=require('node:assert/strict');
const test=require('node:test');
const model=require('../web/map-model.js');
const bounds={x:0,y:0,w:1000,h:700};
test('unavailable or invalid response never displays a clear map',()=>{
 assert.equal(model.levels('N'.repeat(27),false),'?'.repeat(27));
 assert.equal(model.levels('N'.repeat(26),true),'?'.repeat(27));
 assert.equal(model.levels('R'.repeat(27),true),'R'.repeat(27));
 assert.equal(model.levels('N'.repeat(26)+'X',true),'?'.repeat(27));
});
test('zoom preserves the point under the cursor',()=>{
 assert.deepEqual(model.zoom(bounds,2,250,175,bounds),{x:125,y:87.5,w:500,h:350});
});
test('zoom is bounded and reset cannot reveal unbounded space',()=>{
 const tiny=model.zoom(bounds,100,500,350,bounds);
 assert.equal(tiny.w,31.25);
 assert.deepEqual(model.zoom(tiny,0.001,500,350,bounds),bounds);
});
test('panning stays inside geographic bounds',()=>{
 assert.deepEqual(model.pan({x:100,y:100,w:500,h:350},-1000,900,bounds),{x:0,y:350,w:500,h:350});
});
