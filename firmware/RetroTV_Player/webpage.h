// The phone page, stored in the program. Generated from index.html, edit that and regenerate.
#ifndef _WEBPAGE_H_
#define _WEBPAGE_H_
#include <Arduino.h>
const char INDEX_HTML[] PROGMEM = R"HTMLPAGE(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Retro TV</title>
<style>
:root{--bg:#17110f;--card:#231a17;--ink:#f3e9d6;--dim:#a8998a;--red:#c8281f;--line:#3a2c27}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--ink);font:16px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
main{max-width:480px;margin:0 auto;padding:16px}
h1{font-size:22px;margin:4px 0 2px;letter-spacing:.5px}
h1 b{color:var(--red)}
p.sub{margin:0 0 14px;color:var(--dim);font-size:14px}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px;margin-bottom:14px}
h2{font-size:13px;text-transform:uppercase;letter-spacing:1.5px;color:var(--dim);margin:0 0 10px}
label{display:block;font-size:13px;color:var(--dim);margin:10px 0 4px}
input::placeholder{color:#6f6258}
input[type=text],select{width:100%;padding:10px;border-radius:9px;border:1px solid var(--line);background:#120d0b;color:var(--ink);font-size:16px}
input[type=range]{width:100%;accent-color:var(--red)}
.row{display:flex;gap:10px}.row>div{flex:1}
button,.pick{display:block;width:100%;padding:13px;border:0;border-radius:10px;background:var(--red);color:#fff;font-size:17px;font-weight:600;text-align:center;cursor:pointer}
button:disabled{opacity:.45}
button.ghost{background:transparent;border:1px solid var(--line);color:var(--ink);width:auto;padding:7px 12px;font-size:14px;font-weight:500}
.pick input{display:none}
.tv{background:#000;border:6px solid #0c0908;border-radius:12px;overflow:hidden;margin:12px 0 4px;aspect-ratio:4/3}
canvas{display:none}
.tv video{display:block;width:100%;height:100%;object-fit:cover;background:#000}
#bar{height:10px;background:#120d0b;border-radius:6px;overflow:hidden;margin-top:12px}
#bar i{display:block;height:100%;width:0;background:var(--red);transition:width .15s}
#msg{font-size:14px;color:var(--dim);margin:8px 0 0;min-height:20px}
#msg.err{color:#ff8a7a}#msg.ok{color:#9be29b}
ul{list-style:none;margin:0;padding:0}
li{display:flex;align-items:center;gap:8px;padding:9px 0;border-top:1px solid var(--line)}
li:first-child{border-top:0}
li span{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
li small{color:var(--dim)}
li.now span{color:#ffd9a0}
[hidden]{display:none!important}
</style>
</head>
<body>
<main>
<h1>Retro <b>TV</b></h1>
<p class="sub">Pick a video. It gets shrunk and sent to the TV. No computer needed.</p>

<div class="card">
<h2>New clip</h2>
<label class="pick">Choose a video<input id="file" type="file" accept="video/*"></label>
<div id="edit" hidden>
<div class="tv"><video id="v" muted playsinline preload="auto"></video></div>
<label>Clip runs from <b id="st">0.0</b>s to <b id="en">0.0</b>s. Drag to move it.</label>
<input id="start" type="range" min="0" max="0" step="0.1" value="0">
<button id="pv" class="ghost" type="button" style="margin-top:8px">Play preview</button>
<div class="row">
<div><label>Length</label><select id="len"><option>3</option><option selected>5</option><option>8</option><option>10</option><option>15</option><option>20</option><option>30</option></select></div>
<div><label>Smoothness</label><select id="fps"><option value="10">10 fps</option><option value="12" selected>12 fps</option><option value="15">15 fps</option></select></div>
<div><label>Picture</label><select id="fit"><option value="fill">Fill</option><option value="fit">Fit</option></select></div>
</div>
<label>Name</label>
<input id="name" type="text" maxlength="20" placeholder="clip" autocomplete="off" autocapitalize="off" spellcheck="false">
<div style="height:12px"></div>
<button id="go">Convert and send</button>
</div>
<div id="bar" hidden><i></i></div>
<p id="msg"></p>
</div>

<div class="card">
<h2>Brightness</h2>
<input id="lv" type="range" min="10" max="255" step="5" value="255">
</div>

<div class="card">
<h2>On the TV</h2>
<ul id="list"><li><small>Loading...</small></li></ul>
</div>
</main>
<canvas id="cv" width="320" height="240"></canvas>
<script>
var W=320,H=240,MAXF=26000,RATE=2,$=function(i){return document.getElementById(i)};
var v=$('v'),cv=$('cv'),ctx=cv.getContext('2d'),busy=false,defName='clip',want=null,scrubbing=false,pvTimer=0;
function say(t,c){var m=$('msg');m.textContent=t;m.className=c||''}
function bar(p){$('bar').hidden=p<0;$('bar').firstChild.style.width=Math.max(0,Math.min(100,p))+'%'}
function clean(s){return s.replace(/\.[^.]*$/,'').replace(/[^A-Za-z0-9_-]+/g,'_').replace(/^_+|_+$/g,'').slice(0,20)||'clip'}
function once(el,ev,ms){return new Promise(function(r){var d=false,f=function(){if(d)return;d=true;el.removeEventListener(ev,f);r()};el.addEventListener(ev,f);setTimeout(f,ms)})}
function painted(){return new Promise(function(r){var d=false,f=function(){if(!d){d=true;r()}};if(v.requestVideoFrameCallback)v.requestVideoFrameCallback(f);setTimeout(f,70)})}
async function seek(t){var p=once(v,'seeked',2500);v.currentTime=t;await p;await painted()}
function draw(){
  var vw=v.videoWidth,vh=v.videoHeight;if(!vw)return;
  if($('fit').value=='fill'){var s=Math.max(W/vw,H/vh),sw=W/s,sh=H/s;ctx.drawImage(v,(vw-sw)/2,(vh-sh)/2,sw,sh,0,0,W,H)}
  else{ctx.fillStyle='#000';ctx.fillRect(0,0,W,H);var k=Math.min(W/vw,H/vh),dw=vw*k,dh=vh*k;ctx.drawImage(v,0,0,vw,vh,(W-dw)/2,(H-dh)/2,dw,dh)}
}
function jpeg(q){return new Promise(function(r){cv.toBlob(r,'image/jpeg',q)})}
function range(){var t0=+$('start').value;return{t0:t0,len:Math.max(0.1,Math.min(+$('len').value,v.duration-t0))}}
function label(){var r=range();$('st').textContent=r.t0.toFixed(1);$('en').textContent=(r.t0+r.len).toFixed(1)}

// Live preview: while the slider moves, the video jumps to the newest position.
async function scrub(){
  if(busy)return;stopPv();want=+$('start').value;if(scrubbing)return;scrubbing=true;
  while(want!==null){var t=want;want=null;var p=once(v,'seeked',1500);v.currentTime=t;await p}
  scrubbing=false;
}
function stopPv(){if(pvTimer){clearInterval(pvTimer);pvTimer=0;v.pause();$('pv').textContent='Play preview'}}
$('pv').onclick=async function(){
  if(busy)return;if(pvTimer){stopPv();scrub();return}
  var r=range();await seek(r.t0);v.playbackRate=1;try{await v.play()}catch(e){return}
  this.textContent='Stop preview';
  pvTimer=setInterval(function(){if(v.currentTime>=r.t0+r.len-0.03||v.ended){stopPv();v.currentTime=r.t0}},40);
};

$('file').onchange=async function(){
  var f=this.files[0];this.value='';if(!f||busy)return;
  stopPv();say('Opening video...');bar(-1);$('edit').hidden=true;
  if(v.src)URL.revokeObjectURL(v.src);
  var ready=once(v,'loadeddata',8000);
  v.src=URL.createObjectURL(f);v.load();
  try{await v.play();v.pause()}catch(e){}
  await ready;
  if(!v.videoWidth||!isFinite(v.duration)){say('This phone cannot open that video. Try a different one.','err');return}
  $('start').max=Math.max(0,v.duration-1).toFixed(1);$('start').value=0;
  defName=clean(f.name);$('name').value='';$('name').placeholder=defName;
  $('edit').hidden=false;label();say(v.duration.toFixed(1)+' second video. Drag the slider to pick the part you want.');
  await seek(0);
};
$('start').oninput=function(){label();scrub()};
$('len').onchange=function(){label()};
$('fit').onchange=function(){v.style.objectFit=this.value=='fill'?'cover':'contain'};

// Fast way: play the clip (at double speed) and grab frames as they go by. No jumping around.
function capturePlay(t0,n,fps,q,rate,prog){return new Promise(async function(res){
  var out=[],i=0,dups=0,stop=false,last=Date.now(),wd;
  function fin(){if(stop)return;stop=true;clearInterval(wd);v.onended=null;v.pause();v.playbackRate=1;Promise.all(out).then(function(f){res({frames:f,dups:dups})})}
  function cb(now,md){
    if(stop)return;last=Date.now();var m=md.mediaTime,first=true;
    if(i<n&&m>=t0+i/fps-0.01){draw();var p=jpeg(q);while(i<n&&m>=t0+i/fps-0.01){out.push(p);i++;if(!first)dups++;first=false}prog(i)}
    if(i>=n){fin();return}
    v.requestVideoFrameCallback(cb);
  }
  await seek(t0);
  v.requestVideoFrameCallback(cb);v.onended=fin;
  wd=setInterval(function(){if(Date.now()-last>2500)fin()},400);
  v.playbackRate=rate;try{await v.play()}catch(e){fin()}
})}
// A frame too big for the TV's memory gets squeezed until it fits.
async function shrink(b){
  if(!window.createImageBitmap)return b;
  var c=document.createElement('canvas');c.width=W;c.height=H;c.getContext('2d').drawImage(await createImageBitmap(b),0,0);
  var q=0.6;do{b=await new Promise(function(r){c.toBlob(r,'image/jpeg',q)});q-=0.1}while(b.size>MAXF&&q>0.15);
  return b;
}

$('go').onclick=async function(){
  if(busy)return;stopPv();busy=true;this.disabled=true;$('file').disabled=true;
  try{
    var r=range(),fps=+$('fps').value,n=Math.max(1,Math.floor(r.len*fps)),q=0.72,frames=[],i;
    var prog=function(k){bar(60*k/n);say('Converting '+k+' of '+n+' frames')};
    bar(0);prog(0);
    if(v.requestVideoFrameCallback){
      var c=await capturePlay(r.t0,n,fps,q,RATE,prog);
      if(c.dups>n/4){say('Your phone is busy, going a bit slower...');c=await capturePlay(r.t0,n,fps,q,1,prog)}
      frames=c.frames;
    }
    // Careful way, one frame at a time: for anything the fast way missed, and for older browsers.
    for(i=frames.length;i<n;i++){await seek(Math.min(r.t0+i/fps,v.duration-0.05));draw();frames.push(await jpeg(q));if(i%3==0)prog(i+1)}
    var fixed=new Map(),total=0;
    for(i=0;i<n;i++){
      var b=frames[i];if(!b)throw new Error('Could not read that video. Try a different one.');
      if(b.size>MAXF){if(!fixed.has(b))fixed.set(b,await shrink(b));b=frames[i]=fixed.get(b)}
      if(b.size>MAXF+4000)throw new Error('A frame came out too big. Try the Fit picture setting.');
      total+=b.size;
    }
    var blob=new Blob(frames,{type:'application/octet-stream'}),name=clean($('name').value.trim()?$('name').value:defName);
    bar(60);say('Sending '+Math.round(total/1024)+' KB to the TV...');
    await send(blob,name,fps);
    bar(100);say('Done. "'+name+'" is playing on the TV.','ok');
    setTimeout(load,1500);
  }catch(e){say(e.message||String(e),'err');bar(-1)}
  busy=false;this.disabled=false;$('file').disabled=false;
  try{v.currentTime=+$('start').value}catch(e){}
};
function send(blob,name,fps){return new Promise(function(ok,no){
  var x=new XMLHttpRequest(),fd=new FormData();fd.append('file',blob,name+'.mjpeg');
  x.open('POST','/upload?name='+encodeURIComponent(name)+'&fps='+fps+'&size='+blob.size);
  x.upload.onprogress=function(e){if(e.lengthComputable)bar(60+40*e.loaded/e.total)};
  x.onload=function(){x.status==200?ok():no(new Error(x.responseText||'The TV refused the clip.'))};
  x.onerror=function(){no(new Error('Lost connection to the TV. Is your phone still on its WiFi?'))};
  x.send(fd)})}

function nice(n){return n.replace(/\.mjpeg$/,'').replace(/\.f\d+$/,'')}
async function load(){
  try{
    var r=await fetch('/list',{cache:'no-store'}),j=await r.json(),ul=$('list');ul.innerHTML='';
    if(!j.clips.length){ul.innerHTML='<li><small>No clips yet.</small></li>';return}
    j.clips.forEach(function(c){
      var li=document.createElement('li'),s=document.createElement('span'),m=document.createElement('small');
      s.textContent=nice(c.n);m.textContent=Math.round(c.s/1024)+' KB';
      if(c.n==j.now)li.className='now';
      li.appendChild(s);li.appendChild(m);
      li.appendChild(btn('Play',function(){act('/play',c.n)}));
      li.appendChild(btn('Delete',function(){if(confirm('Delete "'+nice(c.n)+'" from the TV?'))act('/delete',c.n)}));
      ul.appendChild(li)});
  }catch(e){$('list').innerHTML='<li><small>Cannot reach the TV.</small></li>'}
}
function btn(t,f){var b=document.createElement('button');b.className='ghost';b.textContent=t;b.onclick=f;return b}
async function act(u,n){try{var r=await fetch(u+'?name='+encodeURIComponent(n),{cache:'no-store'});if(!r.ok)say(await r.text(),'err')}catch(e){say('Cannot reach the TV.','err')}setTimeout(load,1200)}
async function light(q){
  try{
    var r=await fetch('/light'+(q||''),{cache:'no-store'}),j=await r.json();
    if(document.activeElement!=$('lv'))$('lv').value=j.level;
  }catch(e){}
}
$('lv').onchange=function(){light('?level='+this.value)};
load();light();
</script>
</body>
</html>
)HTMLPAGE";
#endif
