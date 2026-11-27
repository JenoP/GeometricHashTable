const fs=require('fs');
const TABLE_SIZE=1000000n, TOL=1e-6, MASK=(1n<<64n)-1n;
const asU64=x=>x&MASK;
function mix64(v){v=asU64(v+0x9E3779B97F4A7C15n);v=asU64(asU64(v^(v>>30n))*0xBF58476D1CE4E5B9n);v=asU64(asU64(v^(v>>27n))*0x94D049BB133111EBn);return asU64(v^(v>>31n));}
function quantize(c){return BigInt(Math.round(c/TOL));}
function hashPoint(p){let h=mix64(asU64(quantize(p[0])));h=mix64(asU64(h^asU64(quantize(p[1]))));h=mix64(asU64(h^asU64(quantize(p[2]))));return h;}
function hashTriangle(t,oi){let h=t.map(hashPoint);if(oi)h.sort((a,b)=>a<b?-1:a>b?1:0);let k=mix64(h[0]);k=mix64(k^h[1]);k=mix64(k^h[2]);return Number((k*TABLE_SIZE)>>64n);}
function key(t){return JSON.stringify(t);}
function lex(a,b){for(let i=0;i<3;i++){if(a[i][0]<b[i][0])return -1;if(a[i][0]>b[i][0])return 1;if(a[i][1]<b[i][1])return -1;if(a[i][1]>b[i][1])return 1;if(a[i][2]<b[i][2])return -1;if(a[i][2]>b[i][2])return 1;}return 0;}
const text=fs.readFileSync('GeometricHashTable/wavy_surface_100000.stl','utf8');
const lines=text.split(/\r?\n/),triangles=[],verts=[];
for(const line of lines){const p=line.trim().split(/\s+/);if(p[0]==='vertex'){verts.push(p.slice(1).map(Number));if(verts.length===3){triangles.push([...verts].sort(lex));verts.length=0;if(triangles.length>=16384)break;}}}
const rt=new Map(),rf=new Map();
for(const t of triangles){const kt=hashTriangle(t,true),kf=hashTriangle(t,false);if(!rt.has(kt))rt.set(kt,t);if(!rf.has(kf))rf.set(kf,t);}
let seed=0x5EED;function rand(){seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed;}
for(const count of [64,1024,16384]){const q=triangles.slice(0,count),order=Array.from({length:q.length},(_,i)=>i);seed=0x5EED;for(let i=order.length-1;i>0;i--){const j=(rand()%(i+1));[order[i],order[j]]=[order[j],order[i]];}let expected=0,actual=0,mismatches=0;for(let i=0;i<1000000;i++){const t=q[order[i%q.length]];const bt=rt.get(hashTriangle(t,true));const bf=rf.get(hashTriangle(t,false));const er=bt!==undefined&&key(bt)==key(t);const ar=bf!==undefined&&key(bf)==key(t);expected+=er?1:0;actual+=ar?1:0;if(er!==ar)mismatches++;}console.log(count,expected,actual,mismatches);}
