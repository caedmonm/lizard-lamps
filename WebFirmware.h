#pragma once
#include <Arduino.h>
const char WebFirmware[] PROGMEM = R"JS(
<script>
(()=>{
  const form=document.getElementById('firmware-form');if(!form)return;
  const file=document.getElementById('firmware-file'),progress=document.getElementById('firmware-progress'),message=document.getElementById('firmware-message'),button=form.querySelector('button');
  form.addEventListener('submit',event=>{
    event.preventDefault();if(button.disabled)return;
    const image=file.files[0];
    if(!image||!image.size||!image.name.endsWith('.bin')){message.textContent='Choose an application .bin file.';return}
    button.disabled=true;file.disabled=true;progress.hidden=false;progress.value=0;
    message.textContent='Uploading firmware. Both lamps will be off until the update finishes. Keep power connected.';
    const xhr=new XMLHttpRequest();xhr.open('POST','/api/firmware');xhr.timeout=150000;
    xhr.setRequestHeader('X-CSRF-Token',form.dataset.csrf);xhr.setRequestHeader('X-Firmware-Size',String(image.size));
    const release=()=>{button.disabled=false;file.disabled=false};
    xhr.upload.onprogress=event=>{if(event.lengthComputable){progress.value=Math.round(event.loaded/event.total*100);if(event.loaded===event.total)message.textContent='Upload sent. Validating firmware… Keep power connected.'}};
    xhr.onload=()=>{
      let data;try{data=JSON.parse(xhr.responseText)}catch{}
      if(xhr.status===200&&data?.restarting){progress.value=100;message.textContent='Firmware installed. Restarting… Open the dashboard again in a few seconds.';return}
      message.textContent=xhr.status===401?'Please sign in again by reopening the dashboard.':data?.error||'Update failed. Reopen the dashboard and retry.';release();
    };
    const lost=()=>{message.textContent='Connection lost or timed out. The update result is unknown. Reopen the dashboard before retrying.';release()};
    xhr.onerror=lost;xhr.ontimeout=lost;xhr.onabort=lost;
    const body=new FormData();body.append('firmware',image);xhr.send(body);
  });
  // The HTML button starts disabled so an incomplete page cannot submit a
  // plain form instead of the authenticated upload above.
  button.disabled=false;
  message.textContent='Ready to upload an application .bin file.';
})();
</script>
)JS";
