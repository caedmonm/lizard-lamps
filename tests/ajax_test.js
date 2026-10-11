// Exercises the embedded client with a small DOM/network mock; no browser/device required.
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync('WebClient.h','utf8').split('<script>')[1].split('</script>')[0];
const status={temperatures:[29.4,null],targets:[30,24],activeLamp:0,power:30,sensorsHealthy:true,dimmerOnline:true,clockValid:true,configured:true,clock:'12:00 BST',wifiConnected:true};
function fixture(connecting=false,admin=false){
 const ids={},temps=[0,1].map(i=>({dataset:{temperature:String(i)},textContent:''})),targets=[0,1].map(i=>({dataset:{target:String(i)},textContent:''}));
 for(const id of ['live-message','live-update','controller-clock','connectivity','active-period','power-state','power-percent','heating-fault','clock-action','connection-error'])ids[id]={hidden:false,textContent:'',style:{}};
 if(connecting){for(const id of ['connection-title','connection-waiting','connection-success','connection-network'])ids[id]={hidden:false,textContent:''};}else ids['power-bar']={style:{}};
 const button={disabled:false},form={action:'http://device/schedule',dataset:{},fields:{csrf:'token'},getAttribute:()=>'/schedule',querySelectorAll:s=>s==='button'?[button]:[],addEventListener:(name,fn)=>{form.submit=fn}};
 const password={value:'',disabled:false,required:false},current={value:'0'},toggle={checked:false,form,addEventListener:(name,fn)=>{toggle.change=fn}};
 if(admin){form.action='http://device/admin';form.getAttribute=()=>'/admin';form.querySelector=s=>s==='[data-admin-password]'?password:current;}
 let fail=false,adminResponse={ok:true,adminProtected:false,reauthenticate:false};const calls=[],timers=[],navigations=[];
 const context={document:{hidden:false,getElementById:id=>ids[id],querySelectorAll:s=>s==='[data-admin-toggle]'?(admin?[toggle]:[]):s==='form'?[form]:s==='[data-temperature]'?temps:s==='[data-target]'?targets:[]},location:{href:'http://device/',assign:url=>navigations.push(url)},URL,URLSearchParams,AbortController,Date,FormData:class{*[Symbol.iterator](){yield*Object.entries(form.fields)}},setTimeout:(fn,delay)=>{timers.push({fn,delay});return timers.length},clearTimeout:()=>{},fetch:async(url,options)=>{calls.push({url,options});if(fail)throw new Error('Offline');return {ok:true,status:200,json:async()=>url==='/api/status'?status:url==='/api/connection'?{connected:true,ssid:'Home',ip:'192.168.1.84'}:url==='/api/config'?{probes:['a','b'],starts:['07:00','19:00']}:admin?adminResponse:{ok:true,next:'/?view=monitor'}}}};
 vm.runInNewContext(source,context);return{ids,temps,targets,form,button,calls,timers,password,current,toggle,navigations,setAdminResponse:value=>{adminResponse=value},setFail:()=>{fail=true}};
}
const flush=()=>new Promise(resolve=>setImmediate(resolve));
(async()=>{
 const f=fixture();await flush();assert.equal(f.temps[0].textContent,'29.4');assert.equal(f.temps[1].textContent,'—');assert.equal(f.targets[0].textContent,'30.0°C');assert.equal(f.ids['power-bar'].style.width,'30%');
 await f.form.submit({preventDefault(){}});assert.equal(f.calls.find(c=>c.url==='/api/schedule').options.method,'POST');assert.equal(f.button.disabled,false);assert.match(f.ids['live-message'].textContent,/saved/);
 status.activeLamp=1;status.targets=[20,18];status.power=0;status.sensorsHealthy=false;f.timers.find(t=>t.delay===2000).fn();await flush();assert.equal(f.ids['active-period'].textContent,'Night · Lamp 2');assert.equal(f.targets[0].textContent,'20.0°C');assert.equal(f.ids['heating-fault'].hidden,false);
 f.setFail();await f.form.submit({preventDefault(){}});assert.match(f.ids['live-message'].textContent,/Offline/);assert.equal(f.form.fields.csrf,'token');assert.equal(f.button.disabled,false);
 const c=fixture(true);await flush();assert.equal(c.ids['connection-title'].textContent,"You're connected.");assert.equal(c.ids['connection-waiting'].hidden,true);assert.equal(c.ids['connection-success'].hidden,false);
 const a=fixture(false,true);await flush();assert.equal(a.password.disabled,true);assert.equal(a.password.required,false);
 a.toggle.checked=true;a.toggle.change();assert.equal(a.password.disabled,false);assert.equal(a.password.required,true);
 a.current.value='1';a.toggle.change();assert.equal(a.password.required,false);
 a.toggle.checked=false;a.toggle.change();a.password.value='old-password';await a.form.submit({preventDefault(){}});
 assert.equal(a.current.value,'0');assert.equal(a.password.value,'');assert.equal(a.password.disabled,true);
 a.toggle.checked=true;a.toggle.change();assert.equal(a.password.required,true);
 a.setAdminResponse({ok:true,adminProtected:true,reauthenticate:true});await a.form.submit({preventDefault(){}});assert.deepEqual(a.navigations,['/']);assert.equal(a.button.disabled,false);
 console.log('AJAX polling, DOM updates, period changes, form saves, errors and connection progress passed with mocks.');
})().catch(error=>{console.error(error);process.exitCode=1});
