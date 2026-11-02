#pragma once
#include <Arduino.h>
const char WebHistory[] PROGMEM = R"JS(
<script>
(()=>{
  const $=id=>document.getElementById(id),chart=$('history-chart');
  if(!chart)return;
  const windowControl=$('history-window'),position=$('history-position'),reading=$('history-reading');
  const retention=48*60*60*1000,interval=5*60*1000;
  const format=t=>new Date(t).toLocaleString('en-GB',{timeZone:'Europe/London',day:'2-digit',month:'short',hour:'2-digit',minute:'2-digit',timeZoneName:'short'});
  let samples=[],visible=[],anchor=Date.now(),end=anchor,latest=true,selected=null,chartWidth=800;
  const duration=()=>Number(windowControl.value)*60*60*1000;
  function svg(tag,attrs={},value){
    const e=document.createElementNS('http://www.w3.org/2000/svg',tag);
    Object.entries(attrs).forEach(([k,v])=>e.setAttribute(k,v));
    if(value!==undefined)e.textContent=value;
    chart.append(e);return e;
  }
  function inspect(index){
    const sample=visible[index];
    chart.querySelectorAll('.history-cursor').forEach(e=>e.remove());
    if(!sample){$('history-detail').textContent='No readings in this period.';return}
    selected=sample.time;reading.value=index;
    const x=52+(sample.time-(end-duration()))/duration()*(chartWidth-68);
    svg('line',{x1:x,x2:x,y1:15,y2:242,class:'history-cursor'});
    $('history-detail').textContent=format(sample.time)+' · Basking: '+(sample.values[0]===null?'unavailable':sample.values[0].toFixed(1)+'°C')+' · Ambient: '+(sample.values[1]===null?'unavailable':sample.values[1].toFixed(1)+'°C');
  }
  function draw(){
    chartWidth=Math.max(300,chart.getBoundingClientRect().width);
    chart.setAttribute('viewBox','0 0 '+chartWidth+' 280');
    const span=duration();end=Math.max(anchor-retention+span,Math.min(anchor,end));
    const start=end-span;
    position.max=(retention-span)/60000;position.value=Math.round((start-(anchor-retention))/60000);
    position.disabled=span===retention;
    $('history-older').disabled=start<=anchor-retention;
    $('history-newer').disabled=end>=anchor;
    $('history-latest').disabled=latest;
    $('history-period').textContent=format(start)+' – '+format(end)+' · Europe/London';
    position.setAttribute('aria-valuetext',$('history-period').textContent);
    visible=samples.filter(s=>s.time>=start&&s.time<=end);
    reading.max=Math.max(0,visible.length-1);reading.disabled=!visible.length;
    chart.replaceChildren();
    const values=visible.flatMap(s=>s.values.filter(v=>v!==null));
    const low=values.length?Math.floor(Math.min(...values)-1):0,high=values.length?Math.ceil(Math.max(...values)+1):40;
    const x=t=>52+(t-start)/span*(chartWidth-68),y=v=>242-(v-low)/(high-low)*227;
    for(let i=0;i<=4;i++){
      const value=low+(high-low)*i/4,top=y(value);
      svg('line',{x1:52,x2:chartWidth-16,y1:top,y2:top,class:'history-grid'});
      svg('text',{x:44,y:top+4,'text-anchor':'end'},value.toFixed(1)+'°');
      const time=start+span*i/4;
      svg('text',{x:x(time),y:268,'text-anchor':i===0?'start':i===4?'end':'middle'},new Date(time).toLocaleTimeString('en-GB',{timeZone:'Europe/London',hour:'2-digit',minute:'2-digit'}));
    }
    for(let probe=0;probe<2;probe++){
      let path='',previous=null;
      visible.forEach(s=>{
        const value=s.values[probe];
        if(value===null){previous=null;return}
        path+=(previous!==null&&s.time-previous<=interval*1.5?'L':'M')+x(s.time).toFixed(2)+' '+y(value).toFixed(2)+' ';
        const dot=svg('circle',{cx:x(s.time),cy:y(value),r:2.5,class:'history-series history-series-'+probe});
        const title=document.createElementNS('http://www.w3.org/2000/svg','title');title.textContent=format(s.time)+' · '+(probe===0?'Basking':'Ambient')+': '+value.toFixed(1)+'°C';dot.append(title);
        previous=s.time;
      });
      svg('path',{d:path,class:'history-series history-series-'+probe,fill:'none','stroke-width':2});
    }
    let index=visible.findIndex(s=>s.time===selected);
    inspect(index<0?visible.length-1:index);
    reading.setAttribute('aria-valuetext',$('history-detail').textContent);
  }
  windowControl.addEventListener('change',()=>{if(latest)end=anchor;draw()});
  position.addEventListener('input',()=>{end=anchor-retention+Number(position.value)*60000+duration();latest=Number(position.value)===Number(position.max);selected=null;draw()});
  $('history-older').addEventListener('click',()=>{latest=false;end-=duration();selected=null;draw()});
  $('history-newer').addEventListener('click',()=>{end=Math.min(anchor,end+duration());latest=end===anchor;selected=null;draw()});
  $('history-latest').addEventListener('click',()=>{latest=true;end=anchor;selected=null;draw()});
  reading.addEventListener('input',()=>{inspect(Number(reading.value));reading.setAttribute('aria-valuetext',$('history-detail').textContent)});
  chart.addEventListener('pointermove',event=>{
    if(!visible.length)return;
    const rect=chart.getBoundingClientRect(),time=end-duration()+((event.clientX-rect.left)/rect.width*chartWidth-52)/(chartWidth-68)*duration();
    let index=0;visible.forEach((s,i)=>{if(Math.abs(s.time-time)<Math.abs(visible[index].time-time))index=i});inspect(index);
  });
  async function refresh(){
    if(!document.hidden){
      const abort=new AbortController(),timeout=setTimeout(()=>abort.abort(),5000);
      try{
        const response=await fetch('/api/history',{credentials:'same-origin',cache:'no-store',signal:abort.signal});
        if(!response.ok)throw new Error(response.status===401?'Please sign in again.':'History could not be loaded.');
        const data=await response.json();anchor=Date.now();if(latest){end=anchor;selected=null}
        samples=data.samples.map(s=>({time:anchor-s[0]*1000,values:s.slice(1)}));draw();
        $('history-message').textContent=samples.length?'Readings are snapshots, not averages. Missing readings appear as gaps.':'Waiting for the first reading. Logging continues while this page is closed.';
      }catch(error){$('history-message').textContent='History unavailable · '+error.message+' Displayed history may be out of date. Retrying automatically.'}
      finally{clearTimeout(timeout)}
    }
    setTimeout(refresh,60000);
  }
  window.addEventListener('resize',draw);
  draw();refresh();
})();
</script>
)JS";
