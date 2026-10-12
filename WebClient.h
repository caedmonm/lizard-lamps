#pragma once
#include <Arduino.h>
const char WebClient[] PROGMEM = R"JS(
<script>
(()=>{
  const $=id=>document.getElementById(id);
  const text=(id,value)=>{if($(id))$(id).textContent=value};
  const message=(value,error=false)=>{const e=$('live-message');if(e){e.hidden=!value;e.textContent=value;e.className='notice '+(error?'error':'success')}};
  async function request(url,options={}){
    const abort=new AbortController(),timeout=setTimeout(()=>abort.abort(),5000);
    try{
      const response=await fetch(url,{...options,credentials:'same-origin',cache:'no-store',signal:abort.signal});
      if(response.status===401)throw new Error('Please sign in again by opening the dashboard address.');
      let data;try{data=await response.json()}catch{throw new Error('Unexpected response. Check your connection to the controller.')}
      if(!response.ok)throw new Error(data.error||'The controller could not complete this request.');
      return data;
    }finally{clearTimeout(timeout)}
  }
  function updateStatus(s){
    document.querySelectorAll('[data-temperature]').forEach(e=>{const v=s.temperatures[Number(e.dataset.temperature)];e.textContent=v===null?'—':Number(v).toFixed(1)});
    document.querySelectorAll('[data-target]').forEach(e=>{const v=s.targets[Number(e.dataset.target)];e.textContent=v===null?'—':Number(v).toFixed(1)+'°C'});
    text('controller-clock',s.clock);text('connectivity',s.wifiConnected?'● Wi-Fi connected':'○ Wi-Fi offline');
    text('active-period',s.activeLamp===0?'Day · Lamp 1':s.activeLamp===1?'Night · Lamp 2':'Waiting for schedule');
    text('power-state',s.power>0?'Heating':'Output off');text('power-percent',s.power+'%');
    if($('power-bar'))$('power-bar').style.width=Math.max(0,Math.min(100,s.power))+'%';
    const fault=!s.sensorsHealthy?'check both temperature probes and temperature limits.':!s.dimmerOnline?'zero-cross detection is unavailable.':!s.clockValid?'waiting for a valid clock. Use your browser time below, or wait for network time.':!s.configured?'finish saving the day and night schedule.':'';
    if($('heating-fault')){$('heating-fault').hidden=!fault;$('heating-fault').textContent=fault?'Heating paused — '+fault:''}
    if($('clock-action'))$('clock-action').hidden=s.clockValid;
    text('live-update','Live · Updated '+new Date().toLocaleTimeString());
  }
  function updateConnection(c){
    text('connection-title',c.connected?"You're connected.":'Finding your network…');
    if($('connection-waiting'))$('connection-waiting').hidden=c.connected;
    if($('connection-success'))$('connection-success').hidden=!c.connected;
    text('connection-network',c.ssid);
    document.querySelectorAll('[data-device-link]').forEach(e=>{e.href=c.ip?'http://'+c.ip+'/':'#';if(e.dataset.deviceLink==='address')e.textContent=c.ip||'Not connected'});
  }
  function updateConfig(c){
    if($('setup-notice'))$('setup-notice').hidden=c.configured;
    document.querySelectorAll('[data-start]').forEach(e=>{const i=Number(e.dataset.start);e.textContent=(i===0?'Day · Lamp 1 · ':'Night · Lamp 2 · ')+c.starts[i]});
    document.querySelectorAll('[data-probe]').forEach(e=>e.textContent=c.probes[Number(e.dataset.probe)]||'Not detected');
    document.querySelectorAll('.timeline').forEach(e=>{
      const minute=t=>Number(t.slice(0,2))*60+Number(t.slice(3));const a=minute(c.starts[0]),b=minute(c.starts[1]);
      e.replaceChildren();const add=(start,end)=>{const span=document.createElement('span');span.style.left=(start/14.4)+'%';span.style.width=((end-start)/14.4)+'%';e.append(span)};
      if(a<b)add(a,b);else{add(0,b);add(a,1440)}
      e.setAttribute('aria-label','Daily lamp schedule: lamp 1 starts at '+c.starts[0]+', lamp 2 starts at '+c.starts[1]);
    });
  }
  document.querySelectorAll('[data-dimming-toggle]').forEach(toggle=>{
    const range=toggle.form.querySelector('[data-dimming-range]'),drop=toggle.form.querySelector('[data-restart-drop]');
    const update=()=>{range.hidden=toggle.value!=='1';drop.hidden=toggle.value==='1';range.querySelector('input').disabled=range.hidden;drop.querySelector('input').disabled=drop.hidden};
    toggle.addEventListener('change',update);update();
  });
  document.querySelectorAll('[data-admin-toggle]').forEach(toggle=>{
    const form=toggle.form,password=form.querySelector('[data-admin-password]'),current=form.querySelector('[data-admin-current]');
    const update=()=>{password.disabled=!toggle.checked;password.required=toggle.checked&&current.value!=='1'};
    toggle.addEventListener('change',update);update();
  });
  document.querySelectorAll('form').forEach(form=>form.addEventListener('submit',async event=>{
    event.preventDefault();if(form.dataset.busy)return;form.dataset.busy='1';
    const buttons=[...form.querySelectorAll('button')];buttons.forEach(b=>b.disabled=true);
    form.querySelectorAll('.epoch').forEach(e=>e.value=Math.floor(Date.now()/1000));
    message('Saving…');
    try{
      const data=await request('/api'+new URL(form.action,location.href).pathname,{method:'POST',body:new URLSearchParams(new FormData(form))});
      if(data.reauthenticate){location.assign('/');return}
      if(form.getAttribute('action')==='/admin'){
        form.querySelector('[data-admin-current]').value=data.adminProtected?'1':'0';
        const password=form.querySelector('[data-admin-password]');password.value='';password.required=false;
      }
      if(data.next==='/connecting'){location.assign(data.next);return}
      if(data.resetting){message('Heating is stopping. The device will erase its data and restart. Join the open LizardLamps hotspot and visit http://192.168.4.1.');form.reset();return}
      message('Your changes have been saved.');
      if(form.getAttribute('action')==='/schedule'||form.getAttribute('action')==='/probes'||form.getAttribute('action')==='/control')updateConfig(await request('/api/config'));
      if($('power-bar'))updateStatus(await request('/api/status'));
    }catch(error){message(error.name==='AbortError'?'The controller did not respond. Check your connection before retrying.':error.message,true)}
    finally{delete form.dataset.busy;buttons.forEach(b=>b.disabled=false)}
  }));
  const connecting=!!$('connection-title'),monitoring=!!$('controller-clock')&&!connecting;
  if(monitoring||connecting){
    let failures=0;
    async function poll(){
      if(!document.hidden){
        try{const data=await request(monitoring?'/api/status':'/api/connection');if(monitoring)updateStatus(data);else updateConnection(data);failures=0;text('connection-error','')}
        catch(error){failures++;text('live-update','Connection lost · displayed readings may be out of date');text('connection-error',error.message)}
      }
      setTimeout(poll,failures?Math.min(15000,3000*failures):2000);
    }
    poll();
  }
})();
</script>
)JS";
