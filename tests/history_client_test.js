const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync('WebHistory.h','utf8').split('<script>')[1].split('</script>')[0];
function element(){return {value:'',max:'',disabled:false,textContent:'',children:[],attrs:{},events:{},setAttribute(k,v){this.attrs[k]=v},append(e){this.children.push(e)},replaceChildren(){this.children=[]},addEventListener(k,fn){this.events[k]=fn},querySelectorAll(){return []},getBoundingClientRect(){return {left:0,width:800}}}}
const ids={};for(const name of ['chart','window','position','reading','detail','period','older','newer','latest','message'])ids['history-'+name]=element();
ids['history-window'].value='6';
const toggles=Array.from({length:6},(_,i)=>Object.assign(element(),{checked:true,dataset:{historySeries:String(i)}}));
let fail=false,data={samples:[[86400,24,20],[21600,30,22],[18000,null,23],[17700,31,24],[0,29,22]]};
data.samples=data.samples.map(s=>[...s,70,0,30,24,20,18,20,18]);
const calls=[],timers=[];
vm.runInNewContext(source,{window:{addEventListener(){}},document:{hidden:false,getElementById:id=>ids[id],createElementNS:()=>element(),querySelectorAll:()=>toggles},Date,AbortController,setTimeout:(fn,delay)=>{timers.push({fn,delay});return timers.length},clearTimeout(){},fetch:async(url,options)=>{calls.push({url,options});return {ok:!fail,status:fail?401:200,json:async()=>data}}});
const flush=()=>new Promise(resolve=>setImmediate(resolve));
// A deterministic network/clock fixture exercises cancellation while waiting
// for headers AND while consuming a response body, without sleeping in tests.
function networkFixture(replies){
 const elements={};for(const name of ['chart','window','position','reading','detail','period','older','newer','latest','message'])elements['history-'+name]=element();
 elements['history-window'].value='6';
 let now=0,id=0;const pending=new Map(),requests=[];
 const schedule=(fn,delay)=>{pending.set(++id,{fn,at:now+delay});return id};
 const cancel=id=>pending.delete(id);
 function wait(delay,signal){return new Promise((resolve,reject)=>{
   const abort=()=>{cancel(timer);reject(signal.reason)};
   const timer=schedule(()=>{signal.removeEventListener('abort',abort);resolve()},delay);
   signal.addEventListener('abort',abort,{once:true});
 })}
 const document={hidden:false,getElementById:id=>elements[id],createElementNS:()=>element(),querySelectorAll:()=>[]};
 vm.runInNewContext(source,{window:{addEventListener(){}},document,Date,AbortController,setTimeout:schedule,clearTimeout:cancel,fetch:async(url,options)=>{
   const reply=replies[requests.length];requests.push({url,options});assert(reply,'Unexpected extra history request');
   await wait(reply.headersAfter||0,options.signal);
   return {ok:true,status:200,json:async()=>{await wait(reply.bodyAfter||0,options.signal);return reply.data}};
 }});
 async function advance(ms){
   const until=now+ms;
   for(;;){
     const next=[...pending].filter(([,t])=>t.at<=until).sort((a,b)=>a[1].at-b[1].at)[0];
     if(!next)break;
     now=next[1].at;pending.delete(next[0]);next[1].fn();await flush();
   }
   now=until;await flush();
 }
 return {ids:elements,requests,advance,document};
}
(async()=>{
 await flush();assert.equal(calls[0].url,'/api/history?paged=1');assert.equal(calls[0].options.credentials,'same-origin');
 assert.match(ids['history-detail'].textContent,/Basking: 29.0°C/);assert.match(ids['history-detail'].textContent,/Lamp 1 output: 70%/);assert.match(ids['history-detail'].textContent,/Ambient target: 18.0°C/);assert.equal(ids['history-newer'].disabled,true);
 const paths=()=>ids['history-chart'].children.filter(e=>'d' in e.attrs);
 assert.equal(paths().length,6);assert.equal((paths()[0].attrs.d.match(/M/g)||[]).length,3); // missing data and delayed readings break lines
 toggles[2].checked=false;toggles[2].events.change();assert.equal(paths().length,5);toggles[2].checked=true;toggles[2].events.change();assert.equal(paths().length,6);
 ids['history-reading'].value='1';ids['history-reading'].events.input();assert.match(ids['history-detail'].textContent,/Basking: unavailable/);
 ids['history-older'].events.click();assert.equal(ids['history-newer'].disabled,false);assert.equal(ids['history-latest'].disabled,false);
 ids['history-latest'].events.click();assert.equal(ids['history-newer'].disabled,true);
 ids['history-window'].value='24';ids['history-window'].events.change();assert.equal(ids['history-position'].disabled,true);assert.equal(ids['history-older'].disabled,true);
 ids['history-window'].value='1';ids['history-window'].events.change();ids['history-position'].value='0';ids['history-position'].events.input();assert.equal(ids['history-older'].disabled,true);assert.equal(ids['history-newer'].disabled,false);
 ids['history-chart'].events.pointermove({clientX:52});assert.match(ids['history-detail'].textContent,/Basking: 24.0°C/);
 ids['history-latest'].events.click();data={samples:[]};await timers.find(t=>t.delay===60000).fn();assert.equal(ids['history-reading'].disabled,true);assert.match(ids['history-message'].textContent,/Waiting/);
 data={samples:[],clockValid:false};await timers.filter(t=>t.delay===60000).at(-1).fn();assert.match(ids['history-message'].textContent,/clock/);
 data={samples:[],storageHealthy:false};await timers.filter(t=>t.delay===60000).at(-1).fn();assert.match(ids['history-message'].textContent,/Flash storage unavailable/);
 fail=true;await timers.filter(t=>t.delay===60000).at(-1).fn();assert.match(ids['history-message'].textContent,/sign in again/);
 const sample=value=>[0,value,22,70,0,30,24,20,18,20,18];
 for(const phase of ['headersAfter','bodyAfter']){
   const slow=networkFixture([{[phase]:6500,data:{samples:[sample(29)]}}]);
   await slow.advance(5000);assert.equal(slow.requests[0].options.signal.aborted,false);
   await slow.advance(1500);assert.match(slow.ids['history-detail'].textContent,/Basking: 29.0°C/);
   await slow.advance(15000);assert.equal(slow.requests[0].options.signal.aborted,false); // Cleared on success.
 }
 // Failed header/body downloads preserve the displayed graph, then retry once
 // and recover; the raw platform-specific AbortError never reaches the user.
 for(const phase of ['headersAfter','bodyAfter']){
   const stalled=networkFixture([{data:{samples:[sample(29)]}},{[phase]:Infinity},{data:{samples:[sample(30)]}}]);
   await stalled.advance(0);const previous=stalled.ids['history-detail'].textContent;
   await stalled.advance(60000);await stalled.advance(15000);
   assert.match(stalled.ids['history-message'].textContent,/took too long.*Retrying automatically/);
   assert.doesNotMatch(stalled.ids['history-message'].textContent,/aborted without reason/);
   assert.equal(stalled.ids['history-detail'].textContent,previous);
   await stalled.advance(60000);assert.equal(stalled.requests.length,3);
   assert.match(stalled.ids['history-detail'].textContent,/Basking: 30.0°C/);
   assert.doesNotMatch(stalled.ids['history-message'].textContent,/unavailable/);
 }
 // Each small request gets its own timeout; changing page timestamps do not
 // shift older readings or duplicate the cursor boundary.
 const epoch=1704153600,cursor=epoch-600;
 const paged=networkFixture([
   {headersAfter:6500,data:{nowEpoch:epoch,nextBefore:cursor,samples:[[600,...sample(28).slice(1)],sample(29)]}},
   {headersAfter:6500,data:{nowEpoch:epoch+2,nextBefore:0,samples:[[1802,...sample(26).slice(1)],[1202,...sample(27).slice(1)]]}}
 ]);
 await paged.advance(6500);assert.equal(paged.requests[1].url,'/api/history?paged=1&before='+cursor);
 await paged.advance(6500);assert.equal(paged.ids['history-reading'].max,3);
 assert.match(paged.ids['history-detail'].textContent,/Basking: 29.0°C/);
 paged.ids['history-reading'].value='0';paged.ids['history-reading'].events.input();
 assert.match(paged.ids['history-detail'].textContent,/Basking: 26.0°C/);
 assert.equal(paged.ids['history-chart'].children.find(e=>'cx' in e.attrs).attrs.cx,684.5);
 paged.document.hidden=true;await paged.advance(60000);assert.equal(paged.requests.length,2);
 console.log('History chart navigation, 24h range, paging, delayed headers/bodies, timeout recovery and authentication errors passed.');
})().catch(error=>{console.error(error);process.exitCode=1});
