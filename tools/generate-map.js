// Run: node tools/generate-map.js. No external packages required.
const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '..');
const data = JSON.parse(fs.readFileSync(path.join(root, 'data/ukraine-oblasts.geojson'), 'utf8'));
const names = [
 ['Херсонська область','Херсон',33.4,46.6],['Волинська область','Волинь',24.8,51.1],
 ['Рівненська область','Рівне',26.6,51.1],['Житомирська область','Житомир',28.5,50.6],
 ['Київська область','Київщина',30.2,50.1],['Чернігівська область','Чернігів',32,51.3],
 ['Сумська область','Суми',34,50.9],['Харківська область','Харків',36.4,49.6],
 ['Луганська область','Луганськ',39,49],['Донецька область','Донецьк',37.8,48],
 ['Запорізька область','Запоріжжя',35.6,47.3],['Львівська область','Львів',24,49.7],
 ['Івано-Франківська область','Ів.-Франківськ',24.6,48.7],['Закарпатська область','Закарпаття',23.2,48.4],
 ['Тернопільська область','Тернопіль',25.6,49.5],['Чернівецька область','Чернівці',26.1,48.2],
 ['Одеська область','Одеса',29.7,46.8],['Миколаївська область','Миколаїв',31.8,47.3],
 ['Автономна Республіка Крим','АР Крим',34.3,45.3],['Вінницька область','Вінниця',28.7,49],
 ['Хмельницька область','Хмельницький',27,49.8],['Черкаська область','Черкаси',31.5,49.2],
 ['Полтавська область','Полтава',33.7,49.6],['Дніпропетровська область','Дніпро',34.8,48.4],
 ['Кіровоградська область','Кропивницький',32,48.4],['м. Київ','Київ',30.52,50.45],
 ['м. Севастополь','Севастополь',33.53,44.6]
];
// Preserve source geometry (including inner rings), project longitude by cos(48deg).
const project = ([lon,lat]) => [(lon-22)*Math.cos(48*Math.PI/180),53-lat];
const wide = s => 'L"'+s.replace(/[^\x20-\x7e]/g,c=>'\\u'+c.charCodeAt(0).toString(16).padStart(4,'0'))+'"';
const num = x => x.toFixed(6);
const rings=[]; const points=[]; let out='#include "MapData.h"\n\n';
if(data.features.length!==27) throw Error('Expected 27 administrative units');
// Dataset uses older ISO codes for several regions; names identify the mapping.
const expectedNames=['Kherson Oblast','Volyn Oblast','Rivne Oblast','Zhytomyr Oblast','Kyiv Oblast','Chernihiv Oblast','Sumy Oblast','Kharkiv Oblast','Luhansk Oblast','Donetsk Oblast','Zaporizhia Oblast','Lviv Oblast','Ivano-Frankivsk Oblast','Zakarpattia Oblast','Ternopil Oblast','Chernivtsi Oblast','Odessa Oblast','Mykolaiv Oblast','Autonomous Republic of Crimea','Vinnytsia Oblast','Khmelnytskyi Oblast','Cherkasy Oblast','Poltava Oblast','Dnipropetrovsk Oblast','Kirovohrad Oblast','Kyiv','Sevastopol'];
data.features.forEach((f,region)=>{
 if(f.properties.shapeName!==expectedNames[region]) throw Error('Source order changed: '+f.properties.shapeName);
 const polygons=f.geometry.type==='Polygon'?[f.geometry.coordinates]:f.geometry.coordinates;
 polygons.forEach(p=>p.forEach(r=>{
  const projected=r.map(project); const id=rings.length;
  rings.push({id,count:projected.length,region}); points.push(...projected);
  out+='static const GeoPoint ring'+id+'[] = {\n'+projected.map(p=>'{'+p.map(num).join(',')+'}').join(',\n')+'\n};\n';
 }));
});
out+='\nconst MapRing MapRings[] = {\n'+rings.map(r=>'{ring'+r.id+','+r.count+','+r.region+'}').join(',\n')+'\n};\n';
out+='const int MapRingCount = sizeof(MapRings)/sizeof(MapRings[0]);\n';
out+='const RegionInfo Regions[27] = {\n'+names.map((n,i)=>'{'+wide(n[0])+','+wide(n[1])+',"'+data.features[i].properties.shapeISO+'",'+project([n[2],n[3]]).map(num).join(',')+'}').join(',\n')+'\n};\n';
for(const [name,axis,fn] of [['MinX',0,Math.min],['MinY',1,Math.min],['MaxX',0,Math.max],['MaxY',1,Math.max]]) out+='const double Map'+name+' = '+num(fn(...points.map(p=>p[axis])))+';\n';
fs.writeFileSync(path.join(root,'src/MapData.cpp'),out);
const regions=names.map((n,i)=>({index:i,name:n[0],shortName:n[1],iso:data.features[i].properties.shapeISO}));
fs.writeFileSync(path.join(root,'data/regions.json'),JSON.stringify(regions,null,2)+'\n');
const webRegions=regions.map((region,i)=>({ ...region,
 label:project([names[i][2],names[i][3]]).map(v=>+v.toFixed(6)),
 rings:data.features[i].geometry.type==='Polygon'
  ?data.features[i].geometry.coordinates.map(r=>r.map(project))
  :data.features[i].geometry.coordinates.flatMap(p=>p.map(r=>r.map(project)))
}));
fs.mkdirSync(path.join(root,'web'),{recursive:true});
fs.writeFileSync(path.join(root,'web/regions.js'),'window.AIRALERT_REGIONS = '+JSON.stringify(webRegions)+';\n');
console.log('Generated '+rings.length+' rings, '+points.length+' vertices, 27 regions.');
