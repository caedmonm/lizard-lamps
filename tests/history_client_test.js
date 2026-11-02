const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync('WebHistory.h','utf8').split('<script>')[1].split('</script>')[0];
function element(){return {value:'',max:'',disabled:false,textContent:'',children:[],attrs:{},events:{},setAttribute(k,v){this.attrs[k]=v},append(e){this.children.push(e)},replaceChildren(){this.children=[]},addEventListener(k,fn){this.events[k]=fn},querySelectorAll(){return []},getBoundingClientRect(){return {left:0,width:800}}}}
const ids={};for(const name of ['chart','window','position','reading','detail','period','older','newer','latest','message'])ids['history-'+name]=element();
ids['history-window'].value='6';
const toggles=Array.from({length:8},(_,i)=>Object.assign(element(),{checked:true,dataset:{historySeries:String(i)}}));
let fail=false,data={samples:[[172800,24,20],[21600,30,22],[18000,null,23],[17700,31,24],[0,29,22]]};
data.samples=data.samples.map(s=>[...s,70,0,30,24,20,18]);
const calls=[],timers=[];
vm.runInNewContext(source,{window:{addEventListener(){}},document:{hidden:false,getElementById:id=>ids[id],createElementNS:()=>element(),querySelectorAll:()=>toggles},Date,AbortController,setTimeout:(fn,delay)=>{timers.push({fn,delay});return timers.length},clearTimeout(){},fetch:async(url,options)=>{calls.push({url,options});return {ok:!fail,status:fail?401:200,json:async()=>data}}});
const flush=()=>new Promise(resolve=>setImmediate(resolve));
(async()=>{
 await flush();assert.equal(calls[0].url,'/api/history');assert.equal(calls[0].options.credentials,'same-origin');
 assert.match(ids['history-detail'].textContent,/Basking: 29.0°C/);assert.match(ids['history-detail'].textContent,/Lamp 1 output: 70%/);assert.match(ids['history-detail'].textContent,/Night ambient target: 18.0°C/);assert.equal(ids['history-newer'].disabled,true);
 const paths=()=>ids['history-chart'].children.filter(e=>'d' in e.attrs);
 assert.equal(paths().length,8);assert.equal((paths()[0].attrs.d.match(/M/g)||[]).length,3); // missing data and delayed readings break lines
 toggles[2].checked=false;toggles[2].events.change();assert.equal(paths().length,7);toggles[2].checked=true;toggles[2].events.change();assert.equal(paths().length,8);
 ids['history-reading'].value='1';ids['history-reading'].events.input();assert.match(ids['history-detail'].textContent,/Basking: unavailable/);
 ids['history-older'].events.click();assert.equal(ids['history-newer'].disabled,false);assert.equal(ids['history-latest'].disabled,false);
 ids['history-latest'].events.click();assert.equal(ids['history-newer'].disabled,true);
 ids['history-window'].value='48';ids['history-window'].events.change();assert.equal(ids['history-position'].disabled,true);assert.equal(ids['history-older'].disabled,true);
 ids['history-window'].value='1';ids['history-window'].events.change();ids['history-position'].value='0';ids['history-position'].events.input();assert.equal(ids['history-older'].disabled,true);assert.equal(ids['history-newer'].disabled,false);
 ids['history-chart'].events.pointermove({clientX:52});assert.match(ids['history-detail'].textContent,/Basking: 24.0°C/);
 ids['history-latest'].events.click();data={samples:[]};await timers.find(t=>t.delay===60000).fn();assert.equal(ids['history-reading'].disabled,true);assert.match(ids['history-message'].textContent,/Waiting/);
 data={samples:[],clockValid:false};await timers.filter(t=>t.delay===60000).at(-1).fn();assert.match(ids['history-message'].textContent,/clock/);
 data={samples:[],storageHealthy:false};await timers.filter(t=>t.delay===60000).at(-1).fn();assert.match(ids['history-message'].textContent,/Flash storage unavailable/);
 fail=true;await timers.filter(t=>t.delay===60000).at(-1).fn();assert.match(ids['history-message'].textContent,/sign in again/);
 console.log('History chart ranges, navigation, inspection, gaps, empty state and authentication errors passed.');
})().catch(error=>{console.error(error);process.exitCode=1});
