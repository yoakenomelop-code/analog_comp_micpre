const VR = require(require('path').join(__dirname,'../web/dsp.js')); const fs = 48000;
function sine(f,a,n){const L=new Float32Array(n),R=new Float32Array(n);for(let i=0;i<n;i++)L[i]=R[i]=a*Math.sin(2*Math.PI*f*i/fs);return[L,R];}
const rms=(b,s,e)=>{let t=0;for(let i=s;i<e;i++)t+=b[i]*b[i];return Math.sqrt(t/(e-s));}; const db=x=>20*Math.log10(x);
const g=(L,f)=>{let re=0,im=0;for(let i=fs/2;i<fs;i++){re+=L[i]*Math.cos(2*Math.PI*f*i/fs);im+=L[i]*Math.sin(2*Math.PI*f*i/fs);}return Math.hypot(re,im);};
// analog response measured vs predicted, each module
for (const [type, cls, p] of [['pre73',VR.Pre73,{}],['fet76',VR.Fet76,{variant:0,input:-40}],['opto3a',VR.Opto3A,{peak:0}]]) {
  const fr=[40,60,2200,12000,18000]; const pred=VR.responseAnalog(type,Object.assign({},VR.DEFAULTS[type],p),fs,fr); const out=[];
  fr.forEach((f,i)=>{const m=new cls(fs); m.setParams(p); const [L,R]=sine(f,0.001,fs); m.process(L,R,fs);
    const base = type==='fet76' ? VR.dbToLin(-40) : 1; out.push(`${f}:${db(rms(L,fs/2,fs)/(0.001*base/Math.SQRT2)).toFixed(2)}/${pred[i].toFixed(2)}`);});
  console.log('analog', type, 'measured/pred', out.join(' '));
}
// analog off = clean (no harmonics), on = harmonics
for (const an of [1,0]) { const m=new VR.Pre73(fs); m.setParams({gain:20, analog:an}); const [L,R]=sine(1000,0.1,fs); m.process(L,R,fs); console.log('73 analog',an,'H2',db(g(L,2000)/g(L,1000)).toFixed(1),'dB'); }
for (const an of [1,0]) { const m=new VR.Fet76(fs); m.setParams({input:20, analog:an, attack:7, release:7}); const [L,R]=sine(1000,0.3,fs); m.process(L,R,fs); console.log('76 analog',an,'H3',db(g(L,3000)/g(L,1000)).toFixed(1),'dB (GR-ripple harmonics remain when off)'); }
// continuous mid freq 2000 Hz +10
{ const p={cont:1, midHz:2000, midGain:10}; const r=VR.response73(Object.assign({},VR.DEFAULTS.pre73,p),fs,[1000,2000,4000]).total; console.log('cont mid @2k', r.map(x=>x.toFixed(1)).join(' '));
  const pc={cont:1,hpfHz:10}; console.log('cont hpf off?', VR.freqs73(Object.assign({},VR.DEFAULTS.pre73,pc)).hpf); }
// multi-slot rack: two 76s + two 73s
{ const rack=new VR.Rack(fs); rack.setSlots([{uid:1,type:'pre73'},{uid:2,type:'fet76',p:{input:10}},{uid:3,type:'fet76',p:{input:10,ratio:4}},{uid:4,type:'pre73',p:{midGain:6}},{uid:5,type:'opto3a'}]);
  const [L,R]=sine(440,0.5,fs); for(let b=0;b<fs;b+=128) rack.process(L.subarray(b,b+128),R.subarray(b,b+128),128);
  console.log('rack meters', JSON.stringify(rack.meters().gr), 'nan', L.some(Number.isNaN));
  rack.setSlots([{uid:5,type:'opto3a'},{uid:3,type:'fet76'},{uid:1,type:'pre73'}]); console.log('reordered, kept instance:', rack.slots.map(s=>s.uid+':'+s.type).join(' ')); }
