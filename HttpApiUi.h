#pragma once

#include <ArduinoJson.h>

// ====== UI API & SPA (single-file) ==========================================
static const char UI_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="cs"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP Meteostanice</title>
<style>
  :root{--bg:#0b1220;--bg2:#0e1626;--card:#121b2e;--muted:#7b8aa1;--txt:#e6eefc;--acc:#4ea8ff;--ok:#18c37e;--warn:#f2c14e;--err:#ff6b6b;}
  *{box-sizing:border-box} html,body{height:100%}
  body{margin:0;font:14px/1.45 system-ui,Segoe UI,Roboto,Ubuntu,Arial;color:var(--txt);background:linear-gradient(180deg,var(--bg),var(--bg2))}
  .wrap{max-width:1100px;margin:0 auto;padding:18px}
  header{display:flex;gap:12px;align-items:center;justify-content:space-between;margin-bottom:14px}
  h1{font-size:20px;margin:0}
  .badge{padding:4px 8px;border-radius:999px;background:#18233b;color:var(--muted);font-size:12px}
  nav{display:flex;gap:8px;flex-wrap:wrap}
  nav button{border:0;background:#16243c;color:#cfe0ff;padding:9px 14px;border-radius:10px;cursor:pointer}
  nav button.active{background:var(--acc);color:#00122b}
  .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(240px,1fr));gap:12px;margin-top:12px}
  .card{background:var(--card);border:1px solid #1b2741;border-radius:14px;padding:14px;box-shadow:0 10px 18px rgba(0,0,0,.25)}
  .muted{color:var(--muted)} .row{display:flex;gap:10px;align-items:center;flex-wrap:wrap}
  .kv{display:grid;grid-template-columns:1fr auto;gap:6px 8px}
  .pill{padding:2px 8px;border-radius:999px;font-size:12px}
  .ok{background:rgba(24,195,126,.18);color:var(--ok)} .warn{background:rgba(242,193,78,.18);color:var(--warn)}
  .err{background:rgba(255,107,107,.18);color:var(--err)}
  .big{font-size:24px;font-weight:700}
  .mono{font-family:ui-monospace,SFMono-Regular,Consolas,Monaco,monospace}
  .btn{border:0;background:#1e2b48;color:#cfe0ff;padding:9px 12px;border-radius:10px;cursor:pointer}
  .btn:hover{filter:brightness(1.1)} .btn.acc{background:var(--acc);color:#002042}
  label{display:block;margin-top:8px;font-size:12px;color:#a7b6cc}
  input,select{width:100%;padding:10px 12px;background:#0f1728;border:1px solid #293757;border-radius:10px;color:#dfe9ff}
  .two{display:grid;grid-template-columns:1fr 1fr;gap:10px}
  .hint{font-size:12px;color:#91a2bf}
  .tabs{margin-top:12px}
  .footer{margin-top:16px;color:#91a2bf;font-size:12px;text-align:center}
  canvas{width:100%;height:160px;background:#0d1424;border-radius:10px;border:1px solid #1b2741}
  .table{width:100%;border-collapse:collapse} .table th,.table td{padding:8px;border-bottom:1px solid #223152}
  .section-title{margin:0 0 8px 0;font-size:13px;letter-spacing:.03em;color:#a9bbd8;text-transform:uppercase}
</style>
</head><body><div class="wrap">
  <header>
    <div>
      <h1 id="title">ESP Meteostanice</h1>
      <div class="muted" id="subtitle">načítám…</div>
    </div>
    <div class="row">
      <span id="wifiBadge" class="badge">Wi-Fi…</span>
      <span id="fwBadge" class="badge">FW…</span>
      <button class="btn" id="rebootBtn">Reboot</button>
    </div>
  </header>

  <nav class="tabs">
    <button data-tab="dash" class="active">Přehled</button>
    <button data-tab="charts">Graf</button>
    <button data-tab="settings">Nastavení</button>
    <button data-tab="tools">Nástroje</button>
    <button data-tab="modbus">Modbus</button>
  </nav>

  <!-- DASH -->
  <section id="tab-dash">
    <div class="grid">
      <div class="card">
        <div class="section-title">Okamžité hodnoty</div>
        <div class="kv">
          <div>Teplota (HTU)</div><div class="big" id="t_htu">–</div>
          <div>Vlhkost (HTU)</div><div class="big" id="rh_htu">–</div>
          <div>Tlak (BMP)</div><div class="big" id="p_bmp">–</div>
          <div>Teplota (BMP)</div><div class="big" id="t_bmp">–</div>
          <div>Osvit (BH1750)</div><div class="big" id="lux">–</div>
        </div>
        <div class="row" style="margin-top:8px">
          <span id="htuBadge" class="pill">HTU</span>
          <span id="bmpBadge" class="pill">BMP</span>
          <span id="bhBadge" class="pill">BH1750</span>
        </div>
      </div>

      <div class="card">
        <div class="section-title">Odvozené veličiny & výška</div>
        <div class="kv">
          <div>QNH</div><div class="big" id="qnh">–</div>
          <div>ALT</div><div class="big" id="alt">–</div>
          <div>Rosný bod</div><div class="big" id="td">–</div>
          <div>Abs. vlhkost</div><div class="big" id="ah">–</div>
          <div>VPD</div><div class="big" id="vpd">–</div>
          <div>ICAO</div><div class="big" id="icao">–</div>
        </div>
        <div class="hint" id="aqHint">AutoQNH: –</div>
      </div>

      <div class="card">
        <div class="section-title">Síť & systém</div>
        <div class="kv">
          <div>Zařízení</div><div id="dev">–</div>
          <div>IP</div><div class="mono" id="ip">–</div>
          <div>RSSI</div><div id="rssi">–</div>
          <div>Uptime</div><div id="uptime">–</div>
          <div>AltMode</div><div id="altmode">–</div>
          <div>Unit ID</div><div id="unitid">–</div>
        </div>
      </div>
    </div>
  </section>

  <!-- CHARTS -->
  <section id="tab-charts" style="display:none">
    <div class="card">
      <div class="section-title">Mini graf (Teplota/ Vlhkost/ Tlak)</div>
      <canvas id="chart"></canvas>
      <div class="hint">Graf je pouze živý náhled (uchovává ~300 posledních vzorků v prohlížeči).</div>
    </div>
  </section>

  <!-- SETTINGS -->
  <section id="tab-settings" style="display:none">
    <div class="grid">
      <div class="card">
        <div class="section-title">Obecné</div>
        <label>Název zařízení<input id="f_name" placeholder="ESP-Meteo"></label>
        <div class="two">
          <div><label>Modbus Unit ID<input id="f_unit" type="number" min="1" max="247"></label></div>
          <div><label>Perioda měření (ms)<input id="f_poll" type="number" min="200"></label></div>
        </div>
        <label>Autotest po startu<select id="f_autotest"><option value="0">Ne</option><option value="1">Ano</option></select></label>
      </div>

      <div class="card">
        <div class="section-title">Výška / QNH</div>
        <div class="two">
          <div><label>Elevace (m AMSL)<input id="f_elev" type="number"></label></div>
          <div><label>QNH (Pa)<input id="f_qnh" type="number"></label></div>
        </div>
        <label>AltMode
          <select id="f_altmode">
            <option value="0">AUTO (QNH→ALT, jinak ELEV→QNH, jinak ISA)</option>
            <option value="1">QNH only (ALT z QNH)</option>
            <option value="2">ISA only (ALT z 101325)</option>
            <option value="3">ELEV only (QNH z elevace, ALT=0)</option>
          </select>
        </label>
      </div>

      <div class="card">
        <div class="section-title">AutoQNH</div>
        <div class="two">
          <div><label>AutoQNH povoleno<select id="f_aq_en"><option value="0">Ne</option><option value="1">Ano</option></select></label></div>
          <div><label>Perioda (h)<input id="f_aq_per" type="number" min="1" max="255"></label></div>
        </div>
        <div class="two">
          <div><label>Manuální ICAO povoleno<select id="f_aq_man"><option value="0">Ne</option><option value="1">Ano</option></select></label></div>
          <div><label>Manuální ICAO (4 znaky)<input id="f_aq_icao" maxlength="4" placeholder="LKPR"></label></div>
        </div>
      </div>
    </div>
    <div class="row" style="margin-top:10px">
      <button id="saveBtn" class="btn acc">Uložit do /config.json</button>
      <button id="applyBtn" class="btn">Použít (bez uložení)</button>
    </div>
    <div class="hint" id="saveHint"></div>
  </section>

  <!-- TOOLS -->
  <section id="tab-tools" style="display:none">
    <div class="grid">
      <div class="card">
        <div class="section-title">Sken sběrnice I²C</div>
        <button id="scanBtn" class="btn">Spustit scan</button>
        <table class="table" style="margin-top:8px">
          <thead><tr><th>Adresa</th><th>Hex</th></tr></thead>
          <tbody id="i2cTable"><tr><td colspan="2" class="muted">Zatím nic…</td></tr></tbody>
        </table>
      </div>
      <div class="card">
        <div class="section-title">Self-test senzorů</div>
        <button id="stBtn" class="btn">Spustit self-test</button>
        <div class="hint" id="stHint">—</div>
      </div>
      <div class="card">
        <div class="section-title">AutoQNH</div>
        <button id="aqRunBtn" class="btn">Spustit AutoQNH nyní</button>
        <div class="hint" id="aqRunHint">—</div>
      </div>
    </div>
  </section>

  <!-- MODBUS -->
  <section id="tab-modbus" style="display:none">
    <div class="card">
      <div class="section-title">Čtení / zápis registrů (holding 0-199)</div>
      <div class="two">
        <div><label>Adresa<input id="mb_addr" type="number" min="0" max="199" value="0"></label></div>
        <div><label>Počet (čtení)<input id="mb_n" type="number" min="1" max="32" value="8"></label></div>
      </div>
      <div class="row" style="margin-top:8px">
        <button id="mbReadBtn" class="btn">Číst</button>
        <div style="width:10px"></div>
        <div class="two" style="flex:1;min-width:260px">
          <input id="mb_w_val" placeholder="Hodnota pro zápis">
          <button id="mbWriteBtn" class="btn">Zapsat</button>
        </div>
      </div>
      <table class="table" style="margin-top:8px">
        <thead><tr><th>Reg</th><th>U16</th></tr></thead>
        <tbody id="mbTable"><tr><td colspan="2" class="muted">—</td></tr></tbody>
      </table>
    </div>
  </section>

  <div class="footer">FW <span id="fwFoot">—</span> • © Meteostanice ESPx • UI běží lokálně v prohlížeči</div>
</div>

<script>
const $=s=>document.querySelector(s); const $$=s=>Array.from(document.querySelectorAll(s));
const fmt = {
  cdeg:(v)=> (v==null?'–':(v/100).toFixed(2)+' °C'),
  cperc:(v)=> (v==null?'–':(v/100).toFixed(2)+' %'),
  pa:(v)=> (v? v.toLocaleString('cs-CZ')+' Pa':'–'),
  lux:(v)=> (v==null?'–': v.toLocaleString('cs-CZ')+' lx'),
  m:(v)=> (v==null?'–': v.toFixed(2)+' m'),
  gm3:(v)=> (v==null?'–': (v/100).toFixed(2)+' g/m³'),
  pa16:(v)=> (v==null?'–': v.toLocaleString('cs-CZ')+' Pa'),
  vpd:(v)=> (v==null?'–': v.toLocaleString('cs-CZ')+' Pa'),
  up:(s)=>{let h=Math.floor(s/3600), m=Math.floor((s%3600)/60), ss=s%60; return `${h}h ${m}m ${ss}s`;}
};

function setBadge(el, ok){
  el.className='pill '+(ok?'ok': 'err'); el.textContent = (el.id.startsWith('wifi')?'Wi-Fi': el.id.replace('Badge','').toUpperCase()) + (ok?' OK':' NOK');
}

let chartCtx, chartData = [];
function initChart(){
  const c = $('#chart');
  chartCtx = c.getContext('2d');
}
function pushChartPoint(st){ // simple canvas line plot
  chartData.push({t:Date.now(), T:st.t_htu, RH:st.rh_htu, P:st.p_bmp});
  if(chartData.length>300) chartData.shift();
  drawChart();
}
function drawChart(){
  const c = $('#chart'), ctx = chartCtx; if(!ctx) return;
  const W=c.width = c.clientWidth, H=c.height = c.clientHeight;
  ctx.clearRect(0,0,W,H); ctx.lineWidth=1.5;
  const keys=[['T','#'],['RH','#'],['P','#']];
  const vals = chartData.map(p=>[p.T/100, p.RH/100, p.P/100]); // scale for display
  const flat = vals.flat(); if(!flat.length) return;
  const min = Math.min(...flat), max = Math.max(...flat);
  const x = (i)=> i*(W/(chartData.length-1||1));
  const y = (v)=> H - (v-min)/(max-min||1)*H;
  [["T",0],["RH",1],["P",2]].forEach(([name,idx])=>{
    ctx.beginPath();
    chartData.forEach((p,i)=>{
      const v = (idx==0?p.T/100: idx==1?p.RH/100: p.P/100);
      if(i==0) ctx.moveTo(x(i), y(v)); else ctx.lineTo(x(i), y(v));
    });
    ctx.stroke();
    ctx.fillStyle='#9fb4d6'; ctx.fillText(name, 8, 14+14*idx);
  });
}

async function api(path, opt){
  const r = await fetch(path, opt);
  if(!r.ok) throw new Error('HTTP '+r.status);
  const ct = r.headers.get('content-type')||'';
  return ct.includes('application/json') ? r.json() : r.text();
}

async function refresh(){
  try{
    const st = await api('/api/status');
    $('#title').textContent = st.deviceName || 'ESP Meteostanice';
    $('#subtitle').textContent = `IP ${st.ip||'—'} • RSSI ${st.rssi??'—'} dBm • uptime ${fmt.up(st.uptime_s||0)}`;
    $('#ip').textContent = st.ip||'—'; $('#rssi').textContent = (st.rssi??'—')+' dBm';
    $('#uptime').textContent = fmt.up(st.uptime_s||0); $('#dev').textContent = st.deviceName||'—';
    $('#unitid').textContent = st.unitId; $('#altmode').textContent = st.altMode;
    $('#fwBadge').textContent = `FW ${st.fw_major}.${st.fw_minor}`;
    $('#fwFoot').textContent = `${st.fw_major}.${st.fw_minor}`;
    $('#wifiBadge').className='badge'; $('#wifiBadge').textContent = `Wi-Fi ${st.wifi_ok?'OK':'NOK'}`;

    // sensors
    $('#t_htu').textContent = fmt.cdeg(st.t_htu);
    $('#rh_htu').textContent = fmt.cperc(st.rh_htu);
    $('#p_bmp').textContent = fmt.pa(st.p_bmp);
    $('#t_bmp').textContent = fmt.cdeg(st.t_bmp);
    $('#lux').textContent   = fmt.lux(st.lux);
    $('#qnh').textContent = fmt.pa16(st.qnh);
    $('#alt').textContent = fmt.m((st.alt_cm??0)/100);
    $('#td').textContent  = fmt.cdeg(st.td);
    $('#ah').textContent  = fmt.gm3(st.ah);
    $('#vpd').textContent = fmt.vpd(st.vpd);
    $('#icao').textContent = st.icao||'—';
    $('#aqHint').textContent = `AutoQNH: last=${st.aq_last_result} • dist=${(st.aq_dist10??0)/10} km • lastUpdate=${st.aq_last_up_s}s`;

    setBadge($('#htuBadge'), st.htu_ok);
    setBadge($('#bmpBadge'), st.bmp_ok);
    setBadge($('#bhBadge'),  st.bh_ok);

    pushChartPoint(st);
  }catch(e){
    $('#subtitle').textContent = 'Chyba načítání /api/status';
  }
}

async function loadConfig(){
  const c = await api('/api/config');
  $('#f_name').value = c.deviceName||'';
  $('#f_unit').value = c.unitId||1;
  $('#f_poll').value = c.pollMs||10000;
  $('#f_autotest').value = c.autoTest?1:0;
  $('#f_elev').value = c.cfgElevation_m||0;
  $('#f_qnh').value  = c.cfgQNH_Pa||0;
  $('#f_altmode').value = c.altMode||0;
  $('#f_aq_en').value = c.autoQNH_enable?1:0;
  $('#f_aq_per').value = c.autoQNH_period_h||8;
  $('#f_aq_man').value = c.autoQNH_manual_en?1:0;
  $('#f_aq_icao').value = (c.autoQNH_manual_icao||'').toUpperCase();
}

async function saveConfig(persist){
  const body = {
    deviceName: $('#f_name').value.trim(),
    unitId: +$('#f_unit').value,
    pollMs: +$('#f_poll').value,
    autoTest: +$('#f_autotest').value,
    cfgElevation_m: +$('#f_elev').value,
    cfgQNH_Pa: +$('#f_qnh').value,
    altMode: +$('#f_altmode').value,
    autoQNH_enable: +$('#f_aq_en').value,
    autoQNH_period_h: +$('#f_aq_per').value,
    autoQNH_manual_en: +$('#f_aq_man').value,
    autoQNH_manual_icao: $('#f_aq_icao').value.trim().toUpperCase(),
    persist: !!persist
  };
  const r = await api('/api/config', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(body)});
  $('#saveHint').textContent = r.msg || (persist?'Uloženo.':'Aplikováno.');
  if(r.reboot) setTimeout(()=>location.reload(), 1000);
  refresh();
}

async function action(cmd){
  const r = await api('/api/action', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({cmd})});
  return r;
}

async function doScan(){
  $('#i2cTable').innerHTML = '<tr><td colspan="2" class="muted">Probíhá…</td></tr>';
  await action('scan');
  // status už obsahuje zrcadlo 0x60.. pro prvních 16
  const st = await api('/api/status');
  const list = (st.i2c||[]);
  if(!list.length){ $('#i2cTable').innerHTML = '<tr><td colspan="2">Nic nenalezeno</td></tr>'; return; }
  $('#i2cTable').innerHTML = list.map(v=>`<tr><td>${v}</td><td class="mono">0x${(+v).toString(16).padStart(2,'0')}</td></tr>`).join('');
}

async function doSelfTest(){
  $('#stHint').textContent='Běží…';
  await action('selftest');
  const st = await api('/api/status');
  $('#stHint').textContent=`HTU=${st.htu_ok?'OK':'NOK'} BH1750=${st.bh_ok?'OK':'NOK'} BMP=${st.bmp_ok?'OK':'NOK'}`;
}

async function doAqRun(){
  $('#aqRunHint').textContent='Běží…';
  await action('aq_run');
  const st = await api('/api/status');
  $('#aqRunHint').textContent=`Výsledek=${st.aq_last_result} ICAO=${st.icao||'—'} QNH=${fmt.pa16(st.qnh)}`;
}

async function mbRead(){
  const addr = +$('#mb_addr').value, n= +$('#mb_n').value;
  const r = await api(`/api/mb?addr=${addr}&n=${n}`);
  $('#mbTable').innerHTML = r.map((v,i)=>`<tr><td>${addr+i}</td><td>${v}</td></tr>`).join('');
}
async function mbWrite(){
  const addr = +$('#mb_addr').value; const val = $('#mb_w_val').value;
  await api('/api/mb',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({addr,val:+val})});
  mbRead();
}

function initTabs(){
  $$('#nav button'); $$('.tabs button').forEach(b=>{
    b.onclick=()=>{
      $$('.tabs button').forEach(x=>x.classList.remove('active')); b.classList.add('active');
      ['dash','charts','settings','tools','modbus'].forEach(t=>$('#tab-'+t).style.display = (b.dataset.tab===t)?'block':'none');
      if(b.dataset.tab==='charts') drawChart();
    };
  });
}

async function boot(){
  initTabs(); initChart();
  $('#rebootBtn').onclick=()=>action('reboot');
  $('#scanBtn').onclick=doScan;
  $('#stBtn').onclick=doSelfTest;
  $('#aqRunBtn').onclick=doAqRun;
  $('#saveBtn').onclick=()=>saveConfig(true);
  $('#applyBtn').onclick=()=>saveConfig(false);
  $('#mbReadBtn').onclick=mbRead;
  $('#mbWriteBtn').onclick=mbWrite;

  await refresh(); await loadConfig();
  setInterval(refresh, 3000);
}
boot();
</script>
</body></html>
)HTML";

// ---------- Helpers to send JSON ----------
static void sendJSON(const String& s){ server.send(200, "application/json; charset=utf-8", s); }

template <typename TJsonDocument>
static void sendJSONDoc(const TJsonDocument& doc){
  String payload;
  payload.reserve(512);
  serializeJson(doc, payload);
  sendJSON(payload);
}

static void sendErr(int code, const String& msg){
  StaticJsonDocument<128> doc;
  doc["error"] = msg;

  String payload;
  serializeJson(doc, payload);
  server.send(code, "application/json; charset=utf-8", payload);
}

// ---------- API: /api/status ---------------------------------------------------
static void apiStatus(){
  // Modbus mirrors / live values
  auto H = [&](uint16_t r){ return mb.Hreg(r); };
  const uint32_t qnh = ((uint32_t)H(32) << 16) | H(33);
  const int32_t alt_cm = (int32_t)(((uint32_t)H(34) << 16) | H(35));
  char icao[5];
  readICAOFromRegs(47, 48, icao);

  const bool wifiConnected = (WiFi.status() == WL_CONNECTED);
  const IPAddress ip = WiFi.localIP();

  StaticJsonDocument<768> doc;
  doc["deviceName"] = CFG.deviceName;
  doc["fw_major"] = FW_MAJOR;
  doc["fw_minor"] = FW_MINOR;
  doc["wifi_ok"] = wifiConnected;
  doc["ip"] = wifiConnected ? ip.toString() : String("0.0.0.0");
  doc["rssi"] = WiFi.RSSI();
  doc["unitId"] = CFG.unitId;
  doc["uptime_s"] = (millis() - bootMillis) / 1000UL;
  doc["altMode"] = (unsigned)mb.Hreg(39);

  // sensors + derived
  doc["htu_ok"] = htu_ok;
  doc["bh_ok"] = bh_ok;
  doc["bmp_ok"] = bmp_ok;
  doc["t_htu"] = (int32_t)mb.Hreg(10);
  doc["rh_htu"] = (uint32_t)mb.Hreg(11);
  doc["t_bmp"] = (int32_t)mb.Hreg(12);
  doc["p_bmp"] = ((uint32_t)H(13) << 16) | H(14);
  doc["lux"] = ((uint32_t)H(15) << 16) | H(16);
  doc["qnh"] = qnh;
  doc["alt_cm"] = alt_cm;
  doc["td"] = (int32_t)H(40);
  doc["ah"] = (uint32_t)H(41);
  doc["vpd"] = (uint32_t)H(42);
  doc["icao"] = icao;

  // AutoQNH info
  doc["aq_last_result"] = (uint32_t)H(45);
  doc["aq_dist10"] = (uint32_t)H(46);
  doc["aq_last_up_s"] = ((uint32_t)H(49) << 16) | H(50);

  // I2C poslední scan sloty (96..111)
  JsonArray i2c = doc.createNestedArray("i2c");
  for (uint8_t i = 0; i < 16; i++) {
    uint16_t a = H(96 + i);
    if (a) i2c.add(a);
  }

  sendJSONDoc(doc);
}

// ---------- API: /api/config (GET/POST) ---------------------------------------
static void apiConfigGet(){
  StaticJsonDocument<384> doc;
  doc["deviceName"] = CFG.deviceName;
  doc["unitId"] = CFG.unitId;
  doc["pollMs"] = CFG.pollMs;
  doc["autoTest"] = CFG.autoTest;
  doc["cfgElevation_m"] = (int)CFG.cfgElevation_m;
  doc["cfgQNH_Pa"] = (unsigned long)CFG.cfgQNH_Pa;
  doc["altMode"] = (unsigned)CFG.altMode;
  doc["autoQNH_enable"] = (unsigned)CFG.autoQNH_enable;
  doc["autoQNH_period_h"] = (unsigned)CFG.autoQNH_period_h;
  doc["autoQNH_manual_en"] = (unsigned)CFG.autoQNH_manual_en;
  doc["autoQNH_manual_icao"] = CFG.autoQNH_manual_icao;
  sendJSONDoc(doc);
}

template <size_t Capacity>
static bool readJsonBody(StaticJsonDocument<Capacity> &doc){
  if(!server.hasArg("plain")) return false;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  return !err;
}

static void applyCfgFromJSON(const StaticJsonDocument<768>& d){
  if (d.containsKey("deviceName"))        CFG.deviceName = String((const char*)d["deviceName"]);
  if (d.containsKey("unitId"))            CFG.unitId = (uint16_t)(unsigned)d["unitId"];
  if (d.containsKey("pollMs"))            CFG.pollMs = (uint32_t)(unsigned long)d["pollMs"];
  if (d.containsKey("autoTest"))          CFG.autoTest = (uint8_t)(unsigned)d["autoTest"];
  if (d.containsKey("cfgElevation_m"))    CFG.cfgElevation_m = (int16_t)(int)d["cfgElevation_m"];
  if (d.containsKey("cfgQNH_Pa"))         CFG.cfgQNH_Pa = (uint32_t)(unsigned long)d["cfgQNH_Pa"];
  if (d.containsKey("altMode"))           CFG.altMode = (uint8_t)(unsigned)d["altMode"];
  if (d.containsKey("autoQNH_enable"))    CFG.autoQNH_enable = (uint8_t)(unsigned)d["autoQNH_enable"];
  if (d.containsKey("autoQNH_period_h"))  CFG.autoQNH_period_h = (uint8_t)(unsigned)d["autoQNH_period_h"];
  if (d.containsKey("autoQNH_manual_en")) CFG.autoQNH_manual_en = (uint8_t)(unsigned)d["autoQNH_manual_en"];
  if (d.containsKey("autoQNH_manual_icao")){
    const char* s = d["autoQNH_manual_icao"]; if (s && strlen(s)==4) strlcpy(CFG.autoQNH_manual_icao, s, sizeof(CFG.autoQNH_manual_icao));
  }
  // promítnout do Modbus mirroru i hostname
  applyConfigToModbusMirror();
}

static void apiConfigPost(){
  StaticJsonDocument<768> d;
  if(!readJsonBody(d)) return sendErr(400,"bad json");

  bool persist = d["persist"] | false;
  applyCfgFromJSON(d);
  if (persist) saveConfigFS();

  StaticJsonDocument<128> resp;
  resp["ok"] = true;
  resp["msg"] = persist ? F("Uloženo do FS.") : F("Aplikováno.");
  sendJSONDoc(resp);
}

// ---------- API: /api/action (POST) -------------------------------------------
static void apiActionPost(){
  StaticJsonDocument<192> d; if(!readJsonBody(d)) return sendErr(400,"bad json");
  const char* cmd = d["cmd"] | "";
  if (!strcmp(cmd,"scan")) {
    i2cScan();
    StaticJsonDocument<64> resp; resp["ok"] = true; sendJSONDoc(resp); return;
  }
  if (!strcmp(cmd,"selftest")) {
    sensorSelfTest();
    StaticJsonDocument<64> resp; resp["ok"] = true; sendJSONDoc(resp); return;
  }
  if (!strcmp(cmd,"aq_run")) {
    autoQNH_RunOnce();
    StaticJsonDocument<64> resp; resp["ok"] = true; sendJSONDoc(resp); return;
  }
  if (!strcmp(cmd,"reboot")) {
    StaticJsonDocument<64> resp;
    resp["ok"] = true;
    resp["reboot"] = true;
    sendJSONDoc(resp);
    delay(200);
    ESP.restart();
    return;
  }
  sendErr(400,"unknown cmd");
}

// ---------- API: /api/mb (GET read / POST write) ------------------------------
static void apiMBGet(){
  int addr = server.hasArg("addr") ? server.arg("addr").toInt() : 0;
  int n    = server.hasArg("n") ? server.arg("n").toInt() : 1; if(n<1) n=1; if(n>32) n=32;
  if (addr<0 || addr>199) return sendErr(400,"addr range");
  StaticJsonDocument<256> doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < n; i++) { arr.add((unsigned)mb.Hreg(addr + i)); }
  sendJSONDoc(doc);
}
static void apiMBPost(){
  StaticJsonDocument<160> d; if(!readJsonBody(d)) return sendErr(400,"bad json");
  int addr = d["addr"] | -1; uint16_t val = (uint16_t)(unsigned)d["val"];
  if (addr<0 || addr>199) return sendErr(400,"addr range");
  mb.Hreg(addr,val);
  StaticJsonDocument<64> resp; resp["ok"] = true; sendJSONDoc(resp);
}

// ---------- Mount routes + SPA ------------------------------------------------
void httpInstallUI(){
  // SPA
  server.on("/ui", [](){ server.send_P(200, "text/html; charset=utf-8", UI_HTML); });
  // Optionally make it default root:
  server.on("/", [](){ server.sendHeader("Location","/ui",true); server.send(302,"text/plain",""); });

  // API
  server.on("/api/status", HTTP_GET, apiStatus);
  server.on("/api/config", HTTP_GET, apiConfigGet);
  server.on("/api/config", HTTP_POST, apiConfigPost);
  server.on("/api/action", HTTP_POST, apiActionPost);
  server.on("/api/mb", HTTP_GET, apiMBGet);
  server.on("/api/mb", HTTP_POST, apiMBPost);
}
