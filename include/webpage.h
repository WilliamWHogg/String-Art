#pragma once
#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>String Art Controller</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',system-ui,sans-serif;background:#1a1a2e;color:#e0e0e0;padding:12px;max-width:800px;margin:0 auto}
h1{text-align:center;color:#e94560;margin-bottom:12px;font-size:1.4em}
.card{background:#16213e;border-radius:10px;padding:14px;margin-bottom:12px;border:1px solid #0f3460}
.card h2{color:#e94560;font-size:1.05em;margin-bottom:8px;border-bottom:1px solid #0f3460;padding-bottom:4px}
.row{display:flex;gap:8px;align-items:center;margin-bottom:8px;flex-wrap:wrap}
.row label{min-width:110px;font-size:.88em;color:#a0a0b0}
input[type=number],input[type=text],select{background:#0f3460;border:1px solid #533483;color:#e0e0e0;border-radius:6px;padding:6px 8px;font-size:.9em;width:100px}
input[type=file]{color:#a0a0b0;font-size:.85em}
button{background:#e94560;color:#fff;border:none;border-radius:6px;padding:8px 16px;cursor:pointer;font-size:.88em;font-weight:600;transition:background .2s}
button:hover{background:#c73550}
button:disabled{background:#444;cursor:not-allowed;color:#888}
button.green{background:#27ae60}button.green:hover{background:#1e8449}
button.orange{background:#e67e22}button.orange:hover{background:#d35400}
button.blue{background:#2980b9}button.blue:hover{background:#1f6fa5}
.badge{display:inline-block;padding:2px 10px;border-radius:10px;font-size:.82em;font-weight:700}
.badge.idle{background:#555;color:#ccc}.badge.running{background:#27ae60;color:#fff}
.badge.paused{background:#e67e22;color:#fff}.badge.homing{background:#2980b9;color:#fff}
.progress-wrap{background:#0f3460;border-radius:8px;height:24px;overflow:hidden;position:relative;margin-top:6px}
.progress-bar{background:linear-gradient(90deg,#e94560,#533483);height:100%;transition:width .4s;border-radius:8px}
.progress-text{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;font-size:.82em;font-weight:600}
#log{background:#0a0f1e;border:1px solid #0f3460;border-radius:8px;height:200px;overflow-y:auto;padding:8px;font-family:'Consolas','Courier New',monospace;font-size:.8em;line-height:1.5;color:#7ec8e3}
#log div{border-bottom:1px solid #111a30;padding:1px 0}
.status-row{display:flex;gap:16px;flex-wrap:wrap;align-items:center}
.status-item{font-size:.9em}.status-item span{font-weight:700;color:#e94560}
.jog-group{display:flex;gap:4px;align-items:center}
.jog-group button{padding:6px 10px;min-width:36px}
</style>
</head>
<body>
<h1>&#x1F9F5; String Art Controller</h1>

<!-- Status -->
<div class="card">
<h2>Status</h2>
<div class="status-row">
 <div class="status-item">State: <span id="stBadge" class="badge idle">IDLE</span></div>
 <div class="status-item">Slot: <span id="stSlot">0</span></div>
 <div class="status-item">Pegs: <span id="stPegs">200</span></div>
 <div class="status-item">Threader: <span id="stHome">Not homed</span></div>
</div>
<div class="status-row" style="margin-top:6px">
 <div class="status-item">TT Angle: <span id="stTTdeg">0.000</span>&deg;</div>
 <div class="status-item">TH Angle: <span id="stTHdeg">0.000</span>&deg;</div>
</div>
</div>

<!-- Configuration -->
<div class="card">
<h2>Configuration</h2>
<div class="row"><label>Number of Pegs</label><input type="number" id="cfgPegs" value="200" min="2" max="1024"><button class="blue" onclick="sendConfig()">Apply</button></div>
<div class="row"><label>TT Gear Ratio</label><input type="number" id="cfgTTgr" value="16.0" step="0.001" min="0.1" max="100">
<label>TT Backlash (&deg;)</label><input type="number" id="cfgTTbl" value="0.12" step="0.01" min="0" max="5"></div>
<div class="row"><label>TT Microsteps</label><select id="cfgTTms"><option>1</option><option>2</option><option>4</option><option>8</option><option>16</option><option>32</option></select>
<label>TH Microsteps</label><select id="cfgTHms"><option>1</option><option>2</option><option selected>4</option><option>8</option><option>16</option><option>32</option></select></div>
<div class="row"><label>TT Speed</label><input type="number" id="cfgTTspd" value="1500" min="1" max="50000">
<label>TT Accel</label><input type="number" id="cfgTTacc" value="2000" min="1" max="100000"></div>
<div class="row"><label>TH Speed</label><input type="number" id="cfgTHspd" value="1000" min="1" max="50000">
<label>TH Accel</label><input type="number" id="cfgTHacc" value="2000" min="1" max="100000"></div>
<div class="row"><label>TH Home Speed</label><input type="number" id="cfgTHhspd" value="30" min="1" max="10000"></div>
</div>

<!-- Manual Control -->
<div class="card">
<h2>Manual Control</h2>
<div class="row"><label>Jog Step (°)</label><input type="number" id="jogDeg" value="1.8" step="0.1" min="0.1" max="360"></div>
<div class="row jog-group">
 <button onclick="jog(-parseFloat(E('jogDeg').value)*10)">&#x25C0;&#x25C0;x10</button>
 <button onclick="jog(-parseFloat(E('jogDeg').value))">&#x25C0; CCW</button>
 <button onclick="jog(parseFloat(E('jogDeg').value))">CW &#x25B6;</button>
 <button onclick="jog(parseFloat(E('jogDeg').value)*10)">x10&#x25B6;&#x25B6;</button>
</div>
<div class="row">
 <label>Go to Slot</label><input type="number" id="gotoSlot" value="0" min="0" max="1023">
 <button class="blue" onclick="goSlot()">Go</button>
 <button class="orange" onclick="zero()">Zero Turntable</button>
 <button class="green" onclick="homeThreader()">Home Threader</button>
</div>
<div class="row">
 <button class="green" onclick="thCmd('up')">Threader Up</button>
 <button class="blue" onclick="thCmd('center')">Threader Center</button>
 <button onclick="thCmd('down')">Threader Down</button>
</div>
<div class="row"><label>Up Pos (&deg;)</label><input type="number" id="thUpPos" value="30" step="0.1" min="-360" max="360">
<button class="blue" onclick="thSet('setUp','thUpPos')">Set</button>
<label>Center (&deg;)</label><input type="number" id="thCenterPos" value="16" step="0.1" min="-360" max="360">
<button class="blue" onclick="thSet('setCenter','thCenterPos')">Set</button></div>
<div class="row"><label>Down Pos (&deg;)</label><input type="number" id="thDownPos" value="60" step="0.1" min="-360" max="360">
<button class="blue" onclick="thSet('setDown','thDownPos')">Set</button></div>
</div>

<!-- Program Upload -->
<div class="card">
<h2>Program</h2>
<div class="row"><label>Commands File</label><input type="file" id="cmdFile" accept=".txt,.csv,.text"></div>
<div class="row"><button class="blue" onclick="uploadFile()">Upload</button><span id="uploadStatus" style="font-size:.85em;color:#888"></span></div>
</div>

<!-- Execution -->
<div class="card">
<h2>Execution</h2>
<div class="row">
 <label>Cmd Delay (s)</label><input type="number" id="dbgDelay" value="0" step="0.1" min="0" max="60">
 <label>Speed %</label><input type="number" id="dbgSpeed" value="100" min="1" max="100">
 <button class="blue" onclick="sendDebug()">Apply</button>
</div>
<div class="row">
 <button class="green" id="btnStart" onclick="ctrlStart()">Start</button>
 <button class="orange" id="btnPause" onclick="ctrlPause()" disabled>Pause</button>
 <button class="green" id="btnResume" onclick="ctrlResume()" disabled>Resume</button>
 <button id="btnStop" onclick="ctrlStop()" disabled>Stop</button>
</div>
<div class="progress-wrap">
 <div class="progress-bar" id="progBar" style="width:0%"></div>
 <div class="progress-text" id="progText">0 / 0</div>
</div>
</div>

<!-- Log -->
<div class="card">
<h2>Log</h2>
<div id="log"></div>
</div>

<script>
const E=id=>document.getElementById(id);
let lastLogId=0;

var _focused=new Set();
document.querySelectorAll&&document.addEventListener('focusin',function(e){_focused.add(e.target.id);});
document.addEventListener('focusout',function(e){_focused.delete(e.target.id);});

function api(method,url,body){
 const opts={method};
 if(body!==undefined){opts.headers={'Content-Type':'application/json'};opts.body=JSON.stringify(body);}
 return fetch(url,opts).then(r=>r.json()).catch(()=>null);
}

function sendConfig(){
 api('POST','/api/config',{
  pegs:+E('cfgPegs').value,
  ttGearRatio:+E('cfgTTgr').value,
  ttBacklash:+E('cfgTTbl').value,
  ttMs:+E('cfgTTms').value,
  thMs:+E('cfgTHms').value,
  ttSpeed:+E('cfgTTspd').value,
  ttAccel:+E('cfgTTacc').value,
  thSpeed:+E('cfgTHspd').value,
  thAccel:+E('cfgTHacc').value,
  thHomeSpeed:+E('cfgTHhspd').value
 });
}
function jog(deg){api('POST','/api/jog',{degrees:deg});}
function goSlot(){api('POST','/api/goto',{slot:+E('gotoSlot').value});}
function zero(){api('POST','/api/zero');}
function homeThreader(){api('POST','/api/home');}
function thCmd(a){api('POST','/api/threader',{action:a});}
function thSet(a,id){api('POST','/api/threader',{action:a,value:+E(id).value});}
function ctrlStart(){api('POST','/api/start');}
function ctrlPause(){api('POST','/api/pause');}
function ctrlResume(){api('POST','/api/resume');}
function ctrlStop(){api('POST','/api/stop');}
function sendDebug(){
 api('POST','/api/debug',{delay:+E('dbgDelay').value,speed:+E('dbgSpeed').value});
}

function uploadFile(){
 const f=E('cmdFile').files[0];
 if(!f){E('uploadStatus').textContent='No file selected';return;}
 E('uploadStatus').textContent='Uploading...';
 const reader=new FileReader();
 reader.onload=function(ev){
  fetch('/api/upload',{method:'POST',headers:{'Content-Type':'text/plain'},body:ev.target.result})
  .then(r=>r.json()).then(j=>{
   E('uploadStatus').textContent=j.ok?'OK: '+j.count+' commands':('Error: '+j.error);
  }).catch(()=>{E('uploadStatus').textContent='Upload failed';});
 };
 reader.readAsText(f);
}

const stateNames=['IDLE','RUNNING','PAUSED','HOMING'];
const stateClasses=['idle','running','paused','homing'];

function pollStatus(){
 api('GET','/api/status').then(d=>{
  if(!d)return;
  const s=d.state||0;
  E('stBadge').textContent=stateNames[s]||'?';
  E('stBadge').className='badge '+stateClasses[s];
  E('stSlot').textContent=d.slot;
  E('stPegs').textContent=d.pegs;
  E('stHome').textContent=d.homed?'Homed':'Not homed';
  E('stTTdeg').textContent=(d.ttDeg||0).toFixed(3);
  E('stTHdeg').textContent=(d.thDeg||0).toFixed(3);
  const cnt=d.cmdCount||0,idx=d.cmdIndex||0;
  const pct=cnt?Math.round(idx*100/cnt):0;
  E('progBar').style.width=pct+'%';
  E('progText').textContent=idx+' / '+cnt;
  E('btnStart').disabled=(s!==0);
  E('btnPause').disabled=(s!==1);
  E('btnResume').disabled=(s!==2);
  E('btnStop').disabled=(s===0);
  if(d.cmdDelay!==undefined && !_focused.has('dbgDelay')) E('dbgDelay').value=d.cmdDelay;
  if(d.speedPct!==undefined && !_focused.has('dbgSpeed')) E('dbgSpeed').value=d.speedPct;
 });
}

function pollLog(){
 api('GET','/api/log?since='+lastLogId).then(d=>{
  if(!d||!d.entries)return;
  const el=E('log');
  d.entries.forEach(e=>{
   const div=document.createElement('div');
   div.textContent=e.msg;
   el.appendChild(div);
   if(e.id>lastLogId) lastLogId=e.id;
  });
  // Keep max ~500 entries in DOM
  while(el.children.length>500) el.removeChild(el.firstChild);
  el.scrollTop=el.scrollHeight;
 });
}

setInterval(pollStatus,500);
setInterval(pollLog,1000);
pollStatus();pollLog();

// Load config from ESP32 on page load
api('GET','/api/config').then(d=>{
 if(!d)return;
 E('cfgPegs').value=d.pegs;
 E('cfgTTgr').value=d.ttGearRatio;
 E('cfgTTbl').value=d.ttBacklash;
 E('cfgTTms').value=d.ttMs;
 E('cfgTHms').value=d.thMs;
 E('cfgTTspd').value=d.ttSpeed;
 E('cfgTTacc').value=d.ttAccel;
 E('cfgTHspd').value=d.thSpeed;
 E('cfgTHacc').value=d.thAccel;
 E('cfgTHhspd').value=d.thHomeSpeed;
 E('thUpPos').value=d.thUpDeg;
 E('thCenterPos').value=d.thCenterDeg;
 E('thDownPos').value=d.thDownDeg;
});
</script>
</body>
</html>
)rawliteral";
