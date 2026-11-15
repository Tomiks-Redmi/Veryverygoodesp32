#pragma once
#include <Arduino.h>

// Веб-панель управления: http://192.168.4.1 или http://verygood.local
static const char WEB_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="ru"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>VeryGood ESP32</title>
<style>
:root{--bg:#0f1218;--card:#181d26;--line:#262d3a;--fg:#e6e9ef;--mut:#8a93a6;--acc:#3fb6ff;--ok:#39d98a;--warn:#ffb020;--bad:#ff4d5e}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
header{display:flex;align-items:center;justify-content:space-between;padding:14px 16px;border-bottom:1px solid var(--line)}
h1{font-size:18px;margin:0}h1 b{color:var(--acc)}#st{color:var(--mut);font-size:13px}
main{max-width:1000px;margin:auto;padding:16px;display:grid;gap:12px;grid-template-columns:repeat(auto-fill,minmax(220px,1fr))}
.c{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px}
.c h3{margin:0 0 6px;font-size:12px;font-weight:600;color:var(--mut);text-transform:uppercase;letter-spacing:.06em}
.v{font-size:34px;font-weight:700;font-variant-numeric:tabular-nums}.v small{font-size:15px;color:var(--mut);font-weight:500}
.s{color:var(--mut);font-size:13px}.w2{grid-column:span 2}.full{grid-column:1/-1}
@media(max-width:520px){.w2{grid-column:auto}}
button,select,input{font:inherit;border-radius:9px;border:1px solid var(--line);background:#222a36;color:var(--fg);padding:8px 12px}
button{cursor:pointer}button:hover{border-color:var(--acc)}button.p{background:var(--acc);color:#001;border:0;font-weight:600}
.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin-top:8px}
canvas{width:100%;height:170px;display:block}
.dot{display:inline-block;width:10px;height:10px;border-radius:50%;background:var(--mut);margin-right:6px}
.on{background:var(--ok)}.al{background:var(--bad);animation:b .5s infinite alternate}@keyframes b{to{opacity:.3}}
.pad{display:grid;grid-template-columns:repeat(3,52px);gap:6px;justify-content:center}.pad button{height:46px;padding:0}
#joy{width:110px;height:110px;border-radius:50%;border:2px solid var(--line);position:relative;margin:auto}
#jd{width:16px;height:16px;border-radius:50%;background:var(--acc);position:absolute;left:47px;top:47px}
label{display:flex;justify-content:space-between;gap:8px;align-items:center;margin:6px 0}label input,label select{width:120px}
#log div{border-bottom:1px solid var(--line);padding:4px 0;font-size:13px}#log span{color:var(--mut);margin-right:8px}
</style></head><body>
<header><h1><b>VeryGood</b> ESP32 <span id="clk" style="color:var(--mut);font-weight:500"></span></h1><div id="st">подключение…</div></header>
<main>
<div class="c"><h3>DS18B20</h3><div class="v" id="ds">--</div><div class="s" id="dsx"></div></div>
<div class="c"><h3>DHT11</h3><div class="v" id="dht">--</div><div class="s" id="dhth"></div></div>
<div class="c"><h3>Сонар</h3><div class="v" id="dist">--</div><div class="s">расстояние</div></div>
<div class="c"><h3>Движение</h3><div class="v" id="mot">--</div><div class="s" id="motx"></div></div>
<div class="c w2"><h3>График</h3>
<div class="row" style="margin:0 0 6px"><select id="hs"><option value="0">DS18B20 °C</option><option value="1">DHT11 °C</option><option value="2">Влажность %</option><option value="3">Расстояние см</option><option value="4">Аналог %</option></select><span class="s" id="hinfo"></span></div>
<canvas id="cv"></canvas></div>
<div class="c"><h3>Охрана</h3><div style="font-size:20px;font-weight:700"><span class="dot" id="secd"></span><span id="sec">--</span></div>
<div class="s" id="secx"></div><div class="row"><button class="p" onclick="cmd('arm')">Поставить</button><button onclick="cmd('disarm')">Снять</button></div></div>
<div class="c"><h3>Реле</h3><div style="font-size:20px;font-weight:700"><span class="dot" id="rd"></span><span id="rel">--</span></div>
<div class="s" id="relx"></div><div class="row"><button class="p" onclick="cmd('relay_on')">Вкл</button><button onclick="cmd('relay_off')">Выкл</button></div></div>
<div class="c"><h3>Пульт</h3><div class="pad">
<span></span><button onclick="ev(1)">▲</button><span></span>
<button onclick="ev(3)">◀</button><button class="p" onclick="ev(5)">OK</button><button onclick="ev(4)">▶</button>
<button onclick="ev(6)">↩</button><button onclick="ev(2)">▼</button><button onclick="cmd('beep')">♪</button></div></div>
<div class="c"><h3>Джойстик</h3><div id="joy"><div id="jd"></div></div><div class="s" id="joyx" style="text-align:center;margin-top:6px"></div></div>
<div class="c w2"><h3>Настройки</h3><form id="f">
<label>Порог «жарко», °C<input name="tempHigh" type="number" step="0.5"></label>
<label>Порог «холодно», °C<input name="tempLow" type="number" step="0.5"></label>
<label>Режим реле<select name="relayMode"><option value="0">Выкл</option><option value="1">Вручную</option><option value="2">Термостат: нагрев</option><option value="3">Термостат: охлаждение</option><option value="4">По тревоге</option><option value="5">Свет по движению</option></select></label>
<label>Цель термостата, °C<input name="thermoTarget" type="number" step="0.5"></label>
<label>Гистерезис, °C<input name="thermoHyst" type="number" step="0.1"></label>
<label>Свет по движению, с<input name="motionLightSec" type="number"></label>
<label>Дистанция тревоги, см<input name="alarmDistCm" type="number"></label>
<label>Задержка постановки, с<input name="armDelaySec" type="number"></label>
<label>Шаг графика<select name="graphStepIdx"><option value="0">1 с</option><option value="1">5 с</option><option value="2">30 с</option><option value="3">1 мин</option><option value="4">5 мин</option></select></label>
<label>Яркость экрана<input name="contrast" type="range" min="0" max="255"></label>
<div class="row"><button class="p" type="submit">Сохранить</button></div></form></div>
<div class="c w2"><h3>WiFi-сеть (роутер)</h3><form id="wf">
<label>SSID<input name="staSsid" style="width:180px"></label>
<label>Пароль<input name="staPass" type="password" style="width:180px" placeholder="без изменений"></label>
<div class="row"><button class="p" type="submit">Подключить</button><span class="s" id="ip"></span></div></form>
<h3 style="margin-top:14px">Обновление прошивки</h3>
<form id="uf"><div class="row"><input type="file" name="fw" accept=".bin"><button type="submit">Загрузить</button><span class="s" id="up"></span></div></form></div>
<div class="c w2"><h3>Часы, будильник, подсветка</h3><form id="af">
<label>Часовой пояс (UTC±)<input name="tzOffset" type="number" min="-12" max="14"></label>
<label>Будильник<select name="alarmClockOn"><option value="0">выкл</option><option value="1">вкл</option></select></label>
<label>Время будильника<input name="alarmTime" type="time"></label>
<label>RGB-лента<select name="ledsMode"><option value="0">выкл</option><option value="1">статус</option><option value="2">радуга</option><option value="3">температура</option><option value="4">огонь</option><option value="5">ночник по движению</option><option value="6">сонар</option><option value="7">музыка</option></select></label>
<label>Яркость ленты<input name="ledsBright" type="range" min="5" max="255"></label>
<label>Светодиодов в ленте<input name="ledsCount" type="number" min="0" max="64"></label>
<label>Серво-радар<select name="servoRadar"><option value="0">выкл</option><option value="1">вкл</option></select></label>
<div class="row"><button class="p" type="submit">Сохранить</button></div></form></div>
<div class="c w2"><h3>Telegram-бот</h3><form id="tf">
<div class="s">1) Создайте бота у @BotFather и вставьте токен. 2) Напишите боту любое сообщение — он привяжется к вашему чату. Нужен интернет через роутер.</div>
<label>Включён<select name="tgOn"><option value="0">нет</option><option value="1">да</option></select></label>
<label>Токен<input name="tgToken" type="password" style="width:180px" placeholder="без изменений"></label>
<label>Chat ID<input name="tgChat" style="width:180px" placeholder="привяжется сам"></label>
<label>Сообщать о движении<select name="tgMotion"><option value="0">нет</option><option value="1">да</option></select></label>
<div class="row"><button class="p" type="submit">Сохранить</button><span class="s" id="tgs"></span></div></form></div>
<div class="c full"><h3>Журнал событий</h3><div id="log"></div></div>
</main>
<script>
const $=id=>document.getElementById(id);let F=false,hid=0;
const T=c=>c==null?'--':(F?c*9/5+32:c).toFixed(1);const U=()=>F?'°F':'°C';
function cmd(c,extra){return fetch('/api/cmd?c='+c+(extra||''),{method:'POST'}).then(tick)}
function ev(e){cmd('ev','&e='+e)}
function fmt(s){if(s<60)return s+' с';if(s<3600)return Math.floor(s/60)+' мин';return Math.floor(s/3600)+' ч'}
async function tick(){try{const d=await (await fetch('/api/data')).json();F=d.fahrenheit;
$('st').textContent='работает '+fmt(d.uptime)+' · RAM '+(d.heap/1024|0)+' КБ'+(d.rssi?' · '+d.rssi+' dBm':'');
$('ds').innerHTML=T(d.dsT)+'<small>'+U()+'</small>';$('dsx').textContent=d.dsCount>1?d.dsAll.map(T).join(' / '):(d.dsCount?'датчиков: 1':'не найден');
$('dht').innerHTML=T(d.dhtT)+'<small>'+U()+'</small>';$('dhth').textContent=d.dhtH==null?'не найден':'влажность '+d.dhtH+'% · точка росы '+T(d.dew)+U();
$('dist').innerHTML=d.dist==null?'--':d.dist.toFixed(1)+'<small>см</small>';
$('mot').textContent=d.motion?'ЕСТЬ':'нет';$('motx').textContent='срабатываний: '+d.motionCount+(d.motionAgo>=0?' · '+fmt(d.motionAgo)+' назад':'');
$('sec').textContent=d.secName+(d.armLeft?' '+d.armLeft+'с':'');$('secd').className='dot '+(d.sec==3?'al':d.sec?'on':'');
$('secx').textContent='тревог: '+d.alarms+' · последняя: '+d.trigger;
$('rel').textContent=d.relay?'ВКЛ':'ВЫКЛ';$('rd').className='dot '+(d.relay?'on':'');$('relx').textContent='режим: '+d.relayModeName+(d.relayMode==2||d.relayMode==3?' · цель '+T(d.target)+U():'');
$('clk').textContent=d.time!='--:--'?d.time:'';$('tgs').textContent='статус: '+d.tg;
$('jd').style.left=(47+d.joyX*0.47)+'px';$('jd').style.top=(47+d.joyY*0.47)+'px';$('joyx').textContent=d.joy?('X '+d.joyX+' · Y '+d.joyY):'не подключён';
}catch(e){$('st').textContent='нет связи'}}
async function hist(){try{const d=await (await fetch('/api/history?id='+hid)).json();draw(d.v,d.step)}catch(e){}}
function draw(v,step){const c=$('cv'),r=devicePixelRatio||1,W=c.clientWidth,H=c.clientHeight;c.width=W*r;c.height=H*r;const g=c.getContext('2d');g.scale(r,r);g.clearRect(0,0,W,H);
const tc=hid<2&&F;const a=v.filter(x=>x!=null).map(x=>tc?x*9/5+32:x);if(!a.length){g.fillStyle='#8a93a6';g.fillText('нет данных',10,20);$('hinfo').textContent='';return}
let mn=Math.min(...a),mx=Math.max(...a);if(mx-mn<1){mn-=.5;mx+=.5}const p=(mx-mn)*.1;mn-=p;mx+=p;
g.strokeStyle='#262d3a';g.fillStyle='#8a93a6';g.font='11px system-ui';for(let i=0;i<=4;i++){const y=8+(H-16)*i/4;g.beginPath();g.moveTo(30,y);g.lineTo(W,y);g.stroke();g.fillText((mx-(mx-mn)*i/4).toFixed(1),0,y+4)}
const X=i=>30+(W-32)*i/Math.max(1,v.length-1),Y=x=>8+(H-16)*(1-((tc?x*9/5+32:x)-mn)/(mx-mn));
g.beginPath();let st=false;v.forEach((x,i)=>{if(x==null){st=false;return}if(!st){g.moveTo(X(i),Y(x));st=true}else g.lineTo(X(i),Y(x))});
g.strokeStyle='#3fb6ff';g.lineWidth=2;g.stroke();
$('hinfo').textContent='мин '+Math.min(...a).toFixed(1)+' · макс '+Math.max(...a).toFixed(1)+' · '+fmt(v.length*step)}
async function log(){try{const d=await (await fetch('/api/log')).json();$('log').innerHTML=d.map(e=>'<div><span>'+fmt(e.ago)+' назад</span>'+e.t.replace(/</g,'&lt;')+'</div>').join('')}catch(e){}}
async function loadSet(){const d=await (await fetch('/api/settings')).json();for(const f of ['f','af','tf'])for(const el of $(f).elements){if(el.name&&d[el.name]!=null)el.value=d[el.name]}$('wf').staSsid.value=d.staSsid}
function post(url,data){return fetch(url,{method:'POST',body:new URLSearchParams(data)})}
$('f').onsubmit=e=>{e.preventDefault();post('/api/settings',new FormData($('f'))).then(()=>{$('f').querySelector('button').textContent='Сохранено ✓';setTimeout(()=>$('f').querySelector('button').textContent='Сохранить',1500)})};
$('af').onsubmit=e=>{e.preventDefault();post('/api/settings',new FormData($('af'))).then(()=>{const b=$('af').querySelector('button');b.textContent='Сохранено ✓';setTimeout(()=>b.textContent='Сохранить',1500)})};
$('tf').onsubmit=e=>{e.preventDefault();const fd=new FormData($('tf'));if(!fd.get('tgToken'))fd.delete('tgToken');post('/api/settings',fd).then(()=>{$('tf').tgToken.value='';const b=$('tf').querySelector('button');b.textContent='Сохранено ✓';setTimeout(()=>b.textContent='Сохранить',1500);setTimeout(loadSet,500)})};
$('wf').onsubmit=e=>{e.preventDefault();post('/api/settings',new FormData($('wf'))).then(()=>$('ip').textContent='подключаюсь…')};
$('uf').onsubmit=e=>{e.preventDefault();const fd=new FormData($('uf'));if(!fd.get('fw').size)return;const x=new XMLHttpRequest();x.open('POST','/update');
x.upload.onprogress=p=>$('up').textContent=Math.round(p.loaded/p.total*100)+'%';x.onload=()=>$('up').textContent=x.responseText;x.send(fd)};
$('hs').onchange=e=>{hid=+e.target.value;hist()};
post('/api/time',{t:Math.floor(Date.now()/1000)});
tick();hist();log();loadSet();setInterval(tick,1000);setInterval(hist,5000);setInterval(log,4000);
</script></body></html>)HTML";
