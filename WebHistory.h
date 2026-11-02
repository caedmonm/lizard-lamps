#pragma once
#include <Arduino.h>
const char WebHistory[] PROGMEM = R"JS(
<script>
(()=>{
  const $=id=>document.getElementById(id),chart=$('history-chart');
  if(!chart)return;
  const windowControl=$('history-window'),position=$('history-position'),reading=$('history-reading');
  const retention=48*60*60*1000,interval=60*1000;
  const format=t=>new Date(t).toLocaleString('en-GB',{timeZone:'Europe/London',day:'2-digit',month:'short',hour:'2-digit',minute:'2-digit',timeZoneName:'short'});
  let samples=[],visible=[],anchor=Date.now(),end=anchor,latest=true,selected=null,chartWidth=800;
  const series=[
    {name:'Basking',color:'#28583e'},{name:'Ambient',color:'#8260ac'},
    {name:'Lamp 1 output',color:'#d06c1b',power:true},{name:'Lamp 2 output',color:'#286bb0',power:true},
    {name:'Day basking target',color:'#639b41'},{name:'Day ambient target',color:'#bc82ad'},
    {name:'Night basking target',color:'#ab8841'},{name:'Night ambient target',color:'#3b9e9d'}
  ];
  const shown=series.map(()=>true);
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
    const x=52+(sample.time-(end-duration()))/duration()*(chartWidth-110);
    svg('line',{x1:x,x2:x,y1:30,y2:242,class:'history-cursor'});
    $('history-detail').textContent=format(sample.time)+' · '+series.map((item,i)=>item.name+': '+(sample.values[i]===null||sample.values[i]===undefined?'unavailable':sample.values[i].toFixed(item.power?0:1)+(item.power?'%':'°C'))).join(' · ');
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
    const values=visible.flatMap(s=>s.values.filter((v,i)=>v!==null&&shown[i]&&!series[i].power));
    const low=values.length?Math.floor(Math.min(...values)-1):0,high=values.length?Math.ceil(Math.max(...values)+1):40;
    const x=t=>52+(t-start)/span*(chartWidth-110),y=v=>242-(v-low)/(high-low)*212;
    svg('text',{x:44,y:12,'text-anchor':'end'},'°C');
    svg('text',{x:chartWidth-48,y:12},'%');
    const ticks=chartWidth<500?2:4;
    for(let i=0;i<=4;i++){
      const value=low+(high-low)*i/4,top=y(value);
      svg('line',{x1:52,x2:chartWidth-58,y1:top,y2:top,class:'history-grid'});
      svg('text',{x:44,y:top+4,'text-anchor':'end'},value.toFixed(1)+'°');
      svg('text',{x:chartWidth-48,y:top+4},String(i*25));
    }
    for(let i=0;i<=ticks;i++){
      const time=start+span*i/ticks;
      svg('text',{x:x(time),y:268,'text-anchor':i===0?'start':i===ticks?'end':'middle'},new Date(time).toLocaleTimeString('en-GB',{timeZone:'Europe/London',hour:'2-digit',minute:'2-digit'}));
    }
    series.forEach((item,probe)=>{
      if(!shown[probe])return;
      let path='',previous=null;
      visible.forEach(s=>{
        const value=s.values[probe];
        if(value===null||value===undefined){previous=null;return}
        const top=item.power?242-value/100*212:y(value);
        const joined=previous!==null&&s.time-previous<=interval*1.5;
        if(joined&&probe>=2)path+='H'+x(s.time).toFixed(2)+' V'+top.toFixed(2)+' ';
        else path+=(joined?'L':'M')+x(s.time).toFixed(2)+' '+top.toFixed(2)+' ';
        if(probe<2&&(visible.length<=120||!joined))svg('circle',{cx:x(s.time),cy:top,r:2,fill:item.color});
        previous=s.time;
      });
      svg('path',{d:path,stroke:item.color,'stroke-dasharray':probe>=4?'6 4':item.power?'2 3':'none',fill:'none','stroke-width':probe<2?2.3:1.5});
    });
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
    const rect=chart.getBoundingClientRect(),time=end-duration()+((event.clientX-rect.left)/rect.width*chartWidth-52)/(chartWidth-110)*duration();
    let index=0;visible.forEach((s,i)=>{if(Math.abs(s.time-time)<Math.abs(visible[index].time-time))index=i});inspect(index);
  });
  async function refresh(){
    if(!document.hidden){
      const abort=new AbortController(),timeout=setTimeout(()=>abort.abort(),5000);
      try{
        const response=await fetch('/api/history',{credentials:'same-origin',cache:'no-store',signal:abort.signal});
        if(!response.ok)throw new Error(response.status===401?'Please sign in again.':'History could not be loaded.');
        const data=await response.json();anchor=data.nowEpoch?data.nowEpoch*1000:Date.now();if(latest){end=anchor;selected=null}
        samples=data.samples.map(s=>({time:anchor-s[0]*1000,values:s.slice(1)}));draw();
        $('history-message').textContent=data.clockValid===false?'Waiting for a valid controller clock. Saved history will appear after time synchronization.':data.storageHealthy===false?'Flash storage unavailable: new readings may be lost on restart.':samples.length?'Readings are snapshots, not averages. Missing readings appear as gaps.':'Waiting for the first reading. Logging continues while this page is closed.';
      }catch(error){$('history-message').textContent='History unavailable · '+error.message+' Displayed history may be out of date. Retrying automatically.'}
      finally{clearTimeout(timeout)}
    }
    setTimeout(refresh,60000);
  }
  document.querySelectorAll('[data-history-series]').forEach(toggle=>toggle.addEventListener('change',()=>{shown[Number(toggle.dataset.historySeries)]=toggle.checked;draw()}));
  window.addEventListener('resize',draw);
  draw();refresh();
})();
</script>
)JS";
