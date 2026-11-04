const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync('WebFirmware.h','utf8').split('<script>')[1].split('</script>')[0];
function fixture(){
 const button={disabled:true},file={files:[{name:'lizard-lamps.ino.bin',size:1200000}],disabled:false},progress={},message={};
 const form={dataset:{csrf:'token'},querySelector:()=>button,addEventListener:(name,fn)=>form.submit=fn};
 const ids={'firmware-form':form,'firmware-file':file,'firmware-progress':progress,'firmware-message':message};
 let xhr;
 class XHR{constructor(){xhr=this;this.headers={};this.upload={}}open(method,url){this.method=method;this.url=url}setRequestHeader(k,v){this.headers[k]=v}send(body){this.body=body}}
 vm.runInNewContext(source,{document:{getElementById:id=>ids[id]},XMLHttpRequest:XHR,FormData:class{append(name,image){this.name=name;this.image=image}}});
 return {button,file,progress,message,form,get xhr(){return xhr}};
}
const event={preventDefault(){}};
const f=fixture();assert.equal(f.button.disabled,false);assert.match(f.message.textContent,/Ready to upload/);f.form.submit(event);
assert.equal(f.xhr.url,'/api/firmware');assert.equal(f.xhr.headers['X-CSRF-Token'],'token');assert.equal(f.xhr.headers['X-Firmware-Size'],'1200000');
assert.equal(f.xhr.body.name,'firmware');assert(f.button.disabled&&f.file.disabled);
f.xhr.upload.onprogress({lengthComputable:true,loaded:1,total:2});assert.equal(f.progress.value,50);
f.xhr.status=200;f.xhr.responseText='{"ok":true,"restarting":true}';f.xhr.onload();assert.match(f.message.textContent,/Restarting/);assert(f.button.disabled);
for(const response of [{status:422,body:'{"error":"Invalid image"}',message:/Invalid image/},{status:401,body:'Authenticate',message:/sign in/}]){
 const g=fixture();g.form.submit(event);g.xhr.status=response.status;g.xhr.responseText=response.body;g.xhr.onload();assert.match(g.message.textContent,response.message);assert(!g.button.disabled&&!g.file.disabled);
}
const lost=fixture();lost.form.submit(event);lost.xhr.onerror();assert.match(lost.message.textContent,/unknown/);assert(!lost.button.disabled);
const invalid=fixture();invalid.file.files=[{name:'bad.txt',size:1}];invalid.form.submit(event);assert.equal(invalid.xhr,undefined);
console.log('Firmware upload headers, progress, restart, authentication, validation errors and uncertain connection result passed.');
