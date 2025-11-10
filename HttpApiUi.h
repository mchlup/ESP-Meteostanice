#pragma once

#include <ArduinoJson.h>

// ====== UI API & SPA (single-file) ==========================================
static const char UI_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="cs"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP Meteostanice</title>
<style>
  :root{--bg:#050a16;--bg2:#091227;--card:#101b32;--card-border:#1b2741;--muted:#7e8ba5;--txt:#f2f6ff;--acc:#4ea8ff;--accent-2:#8c5eff;--ok:#3ad29f;--warn:#f7c55f;--err:#ff6b6b;}
  *{box-sizing:border-box}
  body{margin:0;background:radial-gradient(120% 120% at 50% 0%,rgba(78,168,255,.16),rgba(140,94,255,.08)) linear-gradient(180deg,var(--bg),var(--bg2));color:var(--txt);font:15px/1.55 "Inter",system-ui,-apple-system,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;min-height:100vh;}
  .wrap{max-width:1200px;margin:0 auto;padding:22px 18px 48px;}
  header{display:flex;flex-wrap:wrap;gap:16px;align-items:flex-start;justify-content:space-between;margin-bottom:18px;}
  header h1{margin:0;font-size:28px;letter-spacing:.01em;}
  .muted{color:var(--muted)}
  .title-block{display:flex;flex-direction:column;gap:6px;min-width:260px}
  .meta-row{display:flex;gap:10px;flex-wrap:wrap;font-size:12px;color:var(--muted)}
  .header-actions{display:flex;flex-direction:column;align-items:flex-end;gap:10px;min-width:200px}
  .badge-row{display:flex;gap:8px;flex-wrap:wrap;justify-content:flex-end}
  .badge{padding:6px 12px;border-radius:999px;background:rgba(22,36,60,.72);color:var(--muted);font-size:12px;font-weight:600;border:1px solid rgba(63,96,150,.45);text-transform:uppercase;letter-spacing:.05em}
  .badge.ok{color:#0b321f;background:rgba(58,210,159,.32);border-color:rgba(58,210,159,.5)}
  .badge.err{color:#3a1111;background:rgba(255,107,107,.32);border-color:rgba(255,107,107,.5)}
  .badge.neutral{color:#243247;background:rgba(78,168,255,.18);border-color:rgba(78,168,255,.38)}
  .tabs{display:flex;flex-wrap:wrap;gap:8px;margin:18px 0}
  .tabs button{border:1px solid rgba(45,68,110,.7);background:rgba(16,33,61,.6);color:#cfe0ff;padding:10px 16px;border-radius:12px;cursor:pointer;font-weight:600;letter-spacing:.01em;backdrop-filter:blur(8px);transition:.2s}
  .tabs button:hover{background:rgba(78,168,255,.25);color:#001a3c}
  .tabs button.active{background:var(--acc);color:#001a3c;border-color:rgba(78,168,255,.8);box-shadow:0 8px 24px rgba(78,168,255,.35)}
  .hero-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:14px;margin-bottom:18px}
  .mini-card{background:linear-gradient(160deg,rgba(78,168,255,.18),rgba(16,27,50,.9));border:1px solid rgba(78,168,255,.25);border-radius:16px;padding:16px 18px;display:flex;flex-direction:column;gap:6px;min-height:110px;box-shadow:0 12px 32px rgba(0,0,0,.25)}
  .mini-card:nth-child(2){background:linear-gradient(160deg,rgba(58,210,159,.18),rgba(16,27,50,.9));border-color:rgba(58,210,159,.3)}
  .mini-card:nth-child(3){background:linear-gradient(160deg,rgba(247,197,95,.18),rgba(16,27,50,.9));border-color:rgba(247,197,95,.28)}
  .mini-card:nth-child(4){background:linear-gradient(160deg,rgba(140,94,255,.18),rgba(16,27,50,.9));border-color:rgba(140,94,255,.28)}
  .mini-label{font-size:12px;text-transform:uppercase;letter-spacing:.12em;color:rgba(235,244,255,.7)}
  .mini-value{font-size:30px;font-weight:700;line-height:1.1}
  .mini-sub{font-size:13px;color:rgba(235,244,255,.7)}
  .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:16px}
  .card{background:var(--card);border:1px solid var(--card-border);border-radius:18px;padding:18px 20px;box-shadow:0 16px 38px rgba(3,7,15,.32);display:flex;flex-direction:column;gap:12px}
  .section-title{margin:0;font-size:13px;letter-spacing:.08em;color:#a9bbd8;text-transform:uppercase}
  .kv{display:grid;grid-template-columns:minmax(120px,1fr) minmax(100px,1.2fr);gap:8px 14px;font-size:14px}
  .kv-wide{grid-template-columns:repeat(auto-fit,minmax(140px,1fr))}
  .big{font-size:24px;font-weight:700}
  .mono{font-family:ui-monospace,SFMono-Regular,Consolas,Monaco,monospace}
  .pill{display:inline-flex;align-items:center;gap:6px;padding:4px 10px;border-radius:999px;font-size:12px;font-weight:600;text-transform:uppercase;letter-spacing:.08em;background:rgba(24,35,59,.9);border:1px solid rgba(63,96,150,.4);color:var(--muted)}
  .pill.ok{color:#0b321f;background:rgba(58,210,159,.22);border-color:rgba(58,210,159,.4)}
  .pill.err{color:#3a1111;background:rgba(255,107,107,.22);border-color:rgba(255,107,107,.4)}
  .pill.warn{color:#3a2a05;background:rgba(247,197,95,.22);border-color:rgba(247,197,95,.4)}
  .status-row{display:flex;flex-wrap:wrap;gap:8px}
  label{display:block;font-size:13px;color:#a7b6cc}
  input,select{width:100%;margin-top:6px;padding:11px 12px;background:#0f182c;border:1px solid #293757;border-radius:12px;color:#e4ecff;font-size:14px}
  .form-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:12px}
  .toggle{display:flex;align-items:center;justify-content:space-between;margin-top:12px;padding:12px 14px;background:rgba(14,24,44,.6);border:1px solid rgba(41,58,90,.7);border-radius:14px}
  .toggle-label{font-size:13px;color:#c6d2eb;line-height:1.3;padding-right:12px}
  .toggle input{appearance:none;width:48px;height:26px;border-radius:999px;background:#142236;border:1px solid #253456;position:relative;transition:.2s}
  .toggle input::after{content:"";position:absolute;width:18px;height:18px;border-radius:999px;background:#b8c6e2;top:3px;left:4px;transition:.2s}
  .toggle input:checked{background:var(--acc);border-color:rgba(78,168,255,.75)}
  .toggle input:checked::after{transform:translateX(20px);background:#001a3c}
  .toggle small{display:block;color:var(--muted);font-size:11px;margin-top:4px}
  .btn{border:0;background:var(--acc);color:#001a3c;padding:11px 18px;border-radius:12px;cursor:pointer;font-weight:600;letter-spacing:.02em;transition:.2s;box-shadow:0 12px 24px rgba(78,168,255,.35)}
  .btn:hover{filter:brightness(1.08);transform:translateY(-1px)}
  .btn.ghost{background:rgba(16,33,61,.6);color:#dbe8ff;border:1px solid rgba(71,108,168,.6);box-shadow:none}
  .btn.ghost:hover{background:rgba(78,168,255,.2);color:#001a3c}
  .btn-row{display:flex;flex-wrap:wrap;gap:12px}
  table{width:100%;border-collapse:collapse;font-size:14px}
  th,td{padding:10px;border-bottom:1px solid rgba(41,63,104,.6);text-align:left}
  tbody tr:hover{background:rgba(13,22,38,.65)}
  canvas{width:100%;height:200px;background:#0d1426;border-radius:14px;border:1px solid rgba(31,48,88,.75);box-shadow:inset 0 0 0 1px rgba(9,15,24,.8)}
  .callout{padding:14px 16px;border-radius:14px;border:1px solid rgba(78,168,255,.32);background:rgba(12,24,45,.75);font-size:13px;color:#cfe0ff}
  .feature-list{list-style:none;margin:0;padding:0;display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:12px}
  .feature-list li{padding:14px 16px;border-radius:14px;background:rgba(14,26,46,.7);border:1px solid rgba(45,68,110,.6);box-shadow:0 10px 26px rgba(0,0,0,.25);font-size:13px;line-height:1.45}
  .feature-list strong{display:block;color:#e6f0ff;font-size:14px;margin-bottom:6px}
  .footer{margin-top:28px;color:#8fa2c4;font-size:12px;text-align:center;letter-spacing:.04em}
  @media (max-width:720px){
    header h1{font-size:22px}
    .header-actions{align-items:flex-start}
    .badge-row{justify-content:flex-start}
  }
  @media (max-width:540px){
    .wrap{padding:18px 14px 32px}
    .tabs{gap:6px}
    .tabs button{flex:1 1 45%;text-align:center}
    canvas{height:180px}
  }
</style>
</head><body>
<div class="wrap">
  <header>
    <div class="title-block">
      <h1 id="title">ESP Meteostanice</h1>
      <div id="subtitle" class="muted">Načítám…</div>
      <div class="meta-row">
        <span>Poslední aktualizace <strong id="lastRefresh">—</strong></span>
      </div>
    </div>
    <div class="header-actions">
      <div class="badge-row">
        <span id="wifiBadge" class="badge">Wi-Fi…</span>
        <span id="fwBadge" class="badge neutral">FW…</span>
      </div>
      <button class="btn ghost" id="rebootBtn">Restartovat</button>
    </div>
  </header>

  <nav class="tabs">
    <button data-tab="dash" class="active">Přehled</button>
    <button data-tab="charts">Graf</button>
    <button data-tab="settings">Nastavení</button>
    <button data-tab="tools">Nástroje</button>
    <button data-tab="modbus">Modbus</button>
    <button data-tab="features">Funkce</button>
  </nav>

  <section id="tab-dash">
    <div class="hero-grid">
      <article class="mini-card">
        <span class="mini-label">Teplota</span>
        <span class="mini-value" id="liveTemp">—</span>
        <span class="mini-sub">HTU21D</span>
      </article>
      <article class="mini-card">
        <span class="mini-label">Relativní vlhkost</span>
        <span class="mini-value" id="liveHumidity">—</span>
        <span class="mini-sub">HTU21D</span>
      </article>
      <article class="mini-card">
        <span class="mini-label">Tlak</span>
        <span class="mini-value" id="livePressure">—</span>
        <span class="mini-sub">BMP180</span>
      </article>
      <article class="mini-card">
        <span class="mini-label">Osvit</span>
        <span class="mini-value" id="liveLux">—</span>
        <span class="mini-sub">BH1750</span>
      </article>
    </div>

    <div class="grid">
      <div class="card">
        <div class="section-title">Okamžité hodnoty</div>
        <div class="kv">
          <div>Teplota (HTU)</div><div class="big" id="t_htu">—</div>
          <div>Vlhkost (HTU)</div><div class="big" id="rh_htu">—</div>
          <div>Tlak (BMP)</div><div class="big" id="p_bmp">—</div>
          <div>Teplota (BMP)</div><div class="big" id="t_bmp">—</div>
          <div>Osvit (BH1750)</div><div class="big" id="lux">—</div>
        </div>
        <div class="status-row">
          <span id="htuBadge" class="pill">HTU21D</span>
          <span id="bmpBadge" class="pill">BMP180</span>
          <span id="bhBadge" class="pill">BH1750</span>
        </div>
      </div>

      <div class="card">
        <div class="section-title">Odvozené veličiny &amp; výška</div>
        <div class="kv">
          <div>QNH</div><div class="big" id="qnh">—</div>
          <div>ALT</div><div class="big" id="alt">—</div>
          <div>Rosný bod</div><div class="big" id="td">—</div>
          <div>Abs. vlhkost</div><div class="big" id="ah">—</div>
          <div>VPD</div><div class="big" id="vpd">—</div>
          <div>ICAO</div><div class="big" id="icao">—</div>
        </div>
        <div class="callout" id="aqHint">AutoQNH: —</div>
      </div>

      <div class="card">
        <div class="section-title">Síť &amp; systém</div>
        <div class="kv kv-wide">
          <div>Stav Wi-Fi</div><div id="wifiStatus">—</div>
          <div>IP adresa</div><div class="mono" id="ip">—</div>
          <div>RSSI</div><div id="rssi">—</div>
          <div>Kvalita signálu</div><div id="signalQuality">—</div>
          <div>Uptime</div><div id="uptime">—</div>
          <div>Režim výšky</div><div id="altmode">—</div>
          <div>Modbus Unit ID</div><div id="unitid">—</div>
          <div>FW verze</div><div id="fwVersion">—</div>
        </div>
      </div>

      <div class="card">
        <div class="section-title">Konfigurace ESP</div>
        <div class="kv">
          <div>Název zařízení</div><div id="cfgName">—</div>
          <div>Perioda měření</div><div id="cfgPoll">—</div>
          <div>Autotest po startu</div><div id="cfgAutoTest">—</div>
          <div>Elevace (m)</div><div id="cfgElevation">—</div>
          <div>QNH (Pa)</div><div id="cfgQnh">—</div>
          <div>AutoQNH</div><div id="cfgAutoQnh">—</div>
          <div>AutoQNH perioda</div><div id="cfgAutoQnhPeriod">—</div>
          <div>Manuální ICAO</div><div id="cfgAutoQnhManual">—</div>
        </div>
      </div>

      <div class="card">
        <div class="section-title">AutoQNH stav</div>
        <div class="kv">
          <div>Poslední výsledek</div><div id="aqResult">—</div>
          <div>Vzdálenost METAR</div><div id="aqDistance">—</div>
          <div>Poslední aktualizace</div><div id="aqLastUpdate">—</div>
        </div>
      </div>
    </div>
  </section>

  <section id="tab-charts" style="display:none">
    <div class="card">
      <div class="section-title">Mini graf (Teplota / Vlhkost / Tlak)</div>
      <canvas id="chart"></canvas>
      <div class="callout">Graf uchovává zhruba posledních 300 vzorků uložených v prohlížeči.</div>
    </div>
  </section>

  <section id="tab-settings" style="display:none">
    <div class="grid">
      <div class="card">
        <div class="section-title">Identita &amp; Modbus</div>
        <label>Název zařízení<input id="f_name" placeholder="ESP-Meteo"></label>
        <div class="form-grid">
          <label>Modbus Unit ID<input id="f_unit" type="number" min="1" max="247"></label>
          <label>Perioda měření (ms)<input id="f_poll" type="number" min="200"></label>
        </div>
        <label class="toggle">
          <span class="toggle-label">Autotest po startu<small>Spustí diagnostiku senzorů po každém bootu.</small></span>
          <input id="f_autotest" type="checkbox">
        </label>
      </div>

      <div class="card">
        <div class="section-title">Výška / QNH</div>
        <div class="form-grid">
          <label>Elevace (m AMSL)<input id="f_elev" type="number"></label>
          <label>QNH (Pa)<input id="f_qnh" type="number"></label>
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
        <label class="toggle">
          <span class="toggle-label">AutoQNH povoleno<small>Automaticky stahuje nejbližší METAR a přepočítá tlak.</small></span>
          <input id="f_aq_en" type="checkbox">
        </label>
        <div class="form-grid">
          <label>Perioda (h)<input id="f_aq_per" type="number" min="1" max="255"></label>
          <label>Manuální ICAO (4 znaky)<input id="f_aq_icao" maxlength="4" placeholder="LKPR"></label>
        </div>
        <label class="toggle">
          <span class="toggle-label">Manuální ICAO povoleno<small>Fixuje vybrané letiště místo automatického výběru.</small></span>
          <input id="f_aq_man" type="checkbox">
        </label>
      </div>
    </div>
    <div class="btn-row" style="margin-top:14px">
      <button id="saveBtn" class="btn">Uložit do /config.json</button>
      <button id="applyBtn" class="btn ghost">Použít (bez uložení)</button>
    </div>
    <div class="callout" id="saveHint"></div>
  </section>

  <section id="tab-tools" style="display:none">
    <div class="grid">
      <div class="card">
        <div class="section-title">Sken sběrnice I²C</div>
        <button id="scanBtn" class="btn ghost">Spustit scan</button>
        <table style="margin-top:12px">
          <thead><tr><th>Adresa</th><th>Hex</th></tr></thead>
          <tbody id="i2cTable"><tr><td colspan="2" class="muted">Zatím nic…</td></tr></tbody>
        </table>
      </div>
      <div class="card">
        <div class="section-title">Self-test senzorů</div>
        <button id="stBtn" class="btn ghost">Spustit self-test</button>
        <div class="callout" id="stHint">—</div>
      </div>
      <div class="card">
        <div class="section-title">AutoQNH</div>
        <button id="aqRunBtn" class="btn ghost">Spustit AutoQNH nyní</button>
        <div class="callout" id="aqRunHint">—</div>
      </div>
    </div>
  </section>

  <section id="tab-modbus" style="display:none">
    <div class="card">
      <div class="section-title">Čtení / zápis registrů (holding 0-199)</div>
      <div class="form-grid">
        <label>Adresa<input id="mb_addr" type="number" min="0" max="199" value="0"></label>
        <label>Počet (čtení)<input id="mb_n" type="number" min="1" max="32" value="8"></label>
      </div>
      <div class="btn-row">
        <button id="mbReadBtn" class="btn ghost">Číst</button>
        <div style="flex:1 1 auto"></div>
        <input id="mb_w_val" placeholder="Hodnota pro zápis">
        <button id="mbWriteBtn" class="btn ghost">Zapsat</button>
      </div>
      <table style="margin-top:12px">
        <thead><tr><th>Reg</th><th>U16</th></tr></thead>
        <tbody id="mbTable"><tr><td colspan="2" class="muted">—</td></tr></tbody>
      </table>
    </div>
  </section>

  <section id="tab-features" style="display:none">
    <div class="card">
      <div class="section-title">Hlavní funkce firmware</div>
      <ul class="feature-list">
        <li><strong>Měření &amp; Modbus TCP</strong>Teplota, vlhkost, tlak a osvětlení s mapováním do holding registrů 0-199.</li>
        <li><strong>AutoQNH</strong>Automatická lokalizace a stahování METAR zpráv s přepočtem QNH/ALT.</li>
        <li><strong>WiFiManager portál</strong>Modeless konfigurace dostupná krátkým stiskem tlačítka FLASH.</li>
        <li><strong>Diagnostika</strong>I²C scanner, self-test senzorů a Modbus echo test.</li>
        <li><strong>Self-test &amp; LED</strong>RGB/SINGLE LED vzory, autotest po startu a stavové badge.</li>
        <li><strong>Serial DEBUG konzole</strong>Plné řízení konfigurace přes sériovou linku.</li>
      </ul>
    </div>
    <div class="card">
      <div class="section-title">Tipy</div>
      <div class="callout">Krátkým stiskem tlačítka FLASH aktivujete portál WiFiManageru. Pro rychlou diagnostiku lze kdykoli spustit I²C scan nebo self-test přímo z této webové aplikace.</div>
    </div>
  </section>

  <div class="footer">FW <span id="fwFoot">—</span> • <span id="devFoot">ESP Meteostanice</span> • UI běží lokálně v prohlížeči</div>
</div>

<script>
const $=s=>document.querySelector(s); const $$=s=>Array.from(document.querySelectorAll(s));
const ALT_MODE_LABELS={0:'AUTO – preferuje QNH z AutoQNH',1:'QNH only – ALT z QNH',2:'ISA only – ALT z 101325 Pa',3:'ELEV only – QNH z elevace'};
const AQ_RESULTS={0:'OK',1:'Chyba Wi-Fi',2:'Chyba geolokace',3:'Chyba METAR stanice',4:'Chyba METAR'};
const fmt={
  cdeg:v=>v==null||isNaN(v)?'—':(v/100).toFixed(2)+' °C',
  cdegShort:v=>v==null||isNaN(v)?'—':(v/100).toFixed(1)+' °C',
  cperc:v=>v==null||isNaN(v)?'—':(v/100).toFixed(2)+' %',
  cpercShort:v=>v==null||isNaN(v)?'—':(v/100).toFixed(1)+' %',
  pa:v=>v==null||isNaN(v)?'—':Number(v).toLocaleString('cs-CZ')+' Pa',
  paShort:v=>v==null||isNaN(v)?'—':(v/100).toFixed(1)+' hPa',
  lux:v=>v==null||isNaN(v)?'—':Number(v).toLocaleString('cs-CZ')+' lx',
  luxShort:v=>{if(v==null||isNaN(v))return'—'; if(v>=1000)return (v/1000).toFixed(1)+' klx'; return Number(v).toLocaleString('cs-CZ')+' lx';},
  m:v=>v==null||isNaN(v)?'—':Number(v).toLocaleString('cs-CZ',{maximumFractionDigits:2})+' m',
  m0:v=>v==null||isNaN(v)?'—':Number(v).toLocaleString('cs-CZ',{maximumFractionDigits:0})+' m',
  gm3:v=>v==null||isNaN(v)?'—':(v/100).toFixed(2)+' g/m³',
  vpd:v=>v==null||isNaN(v)?'—':Number(v).toLocaleString('cs-CZ')+' Pa',
  up:s=>{if(s==null||isNaN(s))return'—'; let d=Math.floor(s/86400); let h=Math.floor((s%86400)/3600); let m=Math.floor((s%3600)/60); let ss=s%60; let parts=[]; if(d)parts.push(`${d}d`); if(h||parts.length)parts.push(`${h}h`); if(m||parts.length)parts.push(`${m}m`); parts.push(`${ss}s`); return parts.join(' ');},
  relative:s=>{if(s==null||s<0||isNaN(s))return'—'; if(s<60)return `${s}s`; if(s<3600)return `${Math.floor(s/60)}m ${s%60}s`; if(s<86400)return `${Math.floor(s/3600)}h ${Math.floor((s%3600)/60)}m`; return `${Math.floor(s/86400)}d ${Math.floor((s%86400)/3600)}h`;},
  bool:v=>v?'Zapnuto':'Vypnuto',
  ms:v=>{if(v==null||isNaN(v))return'—'; if(v>=1000){const s=v/1000; const fixed=s%1===0?s.toFixed(0):s.toFixed(2); return fixed.replace('.',',')+' s';} return `${v} ms`;},
  aqResult:v=>AQ_RESULTS[v]||`Kód ${v}`,
  wifi:ok=>ok?'Připojeno':'Odpojeno',
  signal:rssi=>{if(rssi==null||isNaN(rssi))return'—'; return `${rssi} dBm`;},
  signalQuality:rssi=>{if(rssi==null||isNaN(rssi))return'—'; if(rssi>=-55)return'Výborný'; if(rssi>=-65)return'Dobrý'; if(rssi>=-75)return'Uspokojivý'; if(rssi>=-85)return'Hranice'; return'Slabý';}
};

function setSensorBadge(el,label,ok){el.textContent=label;el.className='pill '+(ok?'ok':'err');}

let chartCtx; const chartData=[];
const CHART_SERIES=[
  {key:'T',label:'Teplota (°C)',color:'#4ea8ff',value:p=>p.T/100},
  {key:'RH',label:'Vlhkost (%)',color:'#3ad29f',value:p=>p.RH/100},
  {key:'P',label:'Tlak (hPa)',color:'#f7c55f',value:p=>p.P/100}
];
function initChart(){chartCtx=$('#chart')?.getContext('2d');}
function pushChartPoint(st){chartData.push({t:Date.now(),T:st.t_htu,RH:st.rh_htu,P:st.p_bmp}); if(chartData.length>300)chartData.shift(); drawChart();}
function drawChart(){const c=$('#chart'); const ctx=chartCtx; if(!ctx||!c)return; const W=c.width=c.clientWidth; const H=c.height=c.clientHeight; ctx.clearRect(0,0,W,H); ctx.lineWidth=1.8; ctx.lineJoin='round'; const values=chartData.map(p=>CHART_SERIES.map(s=>s.value(p))).flat().filter(v=>Number.isFinite(v)); if(!values.length)return; const min=Math.min(...values), max=Math.max(...values); const range=max-min||1; const x=i=>i*(W/Math.max(chartData.length-1,1)); const y=v=>H-(v-min)/range*H; ctx.font='12px Inter, system-ui'; ctx.fillStyle='rgba(222,231,255,.82)'; CHART_SERIES.forEach((series,idx)=>{ctx.strokeStyle=series.color; ctx.beginPath(); chartData.forEach((p,i)=>{const v=series.value(p); if(!Number.isFinite(v))return; if(i===0)ctx.moveTo(x(i),y(v)); else ctx.lineTo(x(i),y(v));}); ctx.stroke(); ctx.fillText(series.label,12,18+16*idx);});}

async function api(path,opt){const r=await fetch(path,opt); if(!r.ok)throw new Error('HTTP '+r.status); const ct=r.headers.get('content-type')||''; return ct.includes('application/json')?r.json():r.text();}

function updateAutoQnhInputs(){const en=$('#f_aq_en').checked; $('#f_aq_per').disabled=!en; $('#f_aq_icao').disabled=!en||!$('#f_aq_man').checked; $('#f_aq_man').disabled=!en;}

async function refresh(){
  try{
    const st=await api('/api/status');
    $('#title').textContent=st.deviceName||'ESP Meteostanice';
    $('#subtitle').textContent=`Wi-Fi ${st.wifi_ok?'připojeno':'odpojeno'} • RSSI ${fmt.signal(st.rssi)} • Uptime ${fmt.up(st.uptime_s||0)}`;
    $('#wifiBadge').textContent=st.wifi_ok?`Wi-Fi ${fmt.signalQuality(st.rssi)}`:'Wi-Fi odpojeno';
    $('#wifiBadge').className='badge '+(st.wifi_ok?'ok':'err');
    $('#fwBadge').textContent=`FW ${st.fw_major}.${st.fw_minor}`;
    $('#fwBadge').className='badge neutral';
    $('#fwFoot').textContent=`${st.fw_major}.${st.fw_minor}`;
    $('#fwVersion').textContent=`${st.fw_major}.${st.fw_minor}`;
    $('#devFoot').textContent=st.deviceName||'ESP Meteostanice';
    $('#ip').textContent=st.ip||'—';
    $('#rssi').textContent=fmt.signal(st.rssi);
    $('#signalQuality').textContent=fmt.signalQuality(st.rssi);
    $('#uptime').textContent=fmt.up(st.uptime_s||0);
    $('#wifiStatus').textContent=fmt.wifi(st.wifi_ok);
    $('#altmode').textContent=ALT_MODE_LABELS[st.altMode]||`Režim ${st.altMode}`;
    $('#unitid').textContent=st.unitId??'—';
    $('#cfgName').textContent=st.deviceName||'—';
    $('#cfgPoll').textContent=fmt.ms(st.pollMs);
    $('#cfgAutoTest').textContent=fmt.bool(!!st.autoTest);
    $('#cfgElevation').textContent=fmt.m0(st.cfgElevation_m);
    $('#cfgQnh').textContent=fmt.pa(st.cfgQNH_Pa);
    $('#cfgAutoQnh').textContent=fmt.bool(!!st.autoQNH_enable);
    $('#cfgAutoQnhPeriod').textContent=st.autoQNH_period_h?`${st.autoQNH_period_h} h`:'—';
    const manualIcao = (st.autoQNH_manual_icao||'').toString().trim();
    $('#cfgAutoQnhManual').textContent=st.autoQNH_manual_en ? (manualIcao ? `${manualIcao} (fix)` : 'Zapnuto (bez ICAO)') : 'Vypnuto';

    $('#t_htu').textContent=fmt.cdeg(st.t_htu);
    $('#rh_htu').textContent=fmt.cperc(st.rh_htu);
    $('#p_bmp').textContent=fmt.pa(st.p_bmp);
    $('#t_bmp').textContent=fmt.cdeg(st.t_bmp);
    $('#lux').textContent=fmt.lux(st.lux);
    $('#liveTemp').textContent=fmt.cdegShort(st.t_htu);
    $('#liveHumidity').textContent=fmt.cpercShort(st.rh_htu);
    $('#livePressure').textContent=fmt.paShort(st.p_bmp);
    $('#liveLux').textContent=fmt.luxShort(st.lux);

    $('#qnh').textContent=fmt.pa(st.qnh);
    $('#alt').textContent=fmt.m((st.alt_cm??0)/100);
    $('#td').textContent=fmt.cdeg(st.td);
    $('#ah').textContent=fmt.gm3(st.ah);
    $('#vpd').textContent=fmt.vpd(st.vpd);
    $('#icao').textContent=st.icao||'—';

    const aqAge = st.aq_last_up_s ? (st.uptime_s||0) - st.aq_last_up_s : null;
    const aqAgeText = fmt.relative(aqAge);
    const aqDistanceKm = ((st.aq_dist10??0)/10).toFixed(1);
    $('#aqHint').textContent=`AutoQNH: ${fmt.aqResult(st.aq_last_result)} • vzdálenost ${aqDistanceKm} km • aktualizace ${aqAgeText}`;
    $('#aqResult').textContent=fmt.aqResult(st.aq_last_result);
    $('#aqDistance').textContent=st.aq_dist10?`${(st.aq_dist10/10).toFixed(1)} km`:'—';
    $('#aqLastUpdate').textContent=aqAgeText==='—'?'—':`${aqAgeText} zpět`;

    setSensorBadge($('#htuBadge'),'HTU21D',st.htu_ok);
    setSensorBadge($('#bmpBadge'),'BMP180',st.bmp_ok);
    setSensorBadge($('#bhBadge'),'BH1750',st.bh_ok);

    pushChartPoint(st);
    $('#lastRefresh').textContent=new Date().toLocaleTimeString('cs-CZ');
  }catch(e){
    $('#subtitle').textContent='Chyba načítání /api/status';
    $('#wifiBadge').textContent='Wi-Fi —';
    $('#wifiBadge').className='badge';
  }
}

async function loadConfig(){
  const c=await api('/api/config');
  $('#f_name').value=c.deviceName||'';
  $('#f_unit').value=c.unitId||1;
  $('#f_poll').value=c.pollMs||10000;
  $('#f_autotest').checked=!!c.autoTest;
  $('#f_elev').value=c.cfgElevation_m||0;
  $('#f_qnh').value=c.cfgQNH_Pa||0;
  $('#f_altmode').value=c.altMode||0;
  $('#f_aq_en').checked=!!c.autoQNH_enable;
  $('#f_aq_per').value=c.autoQNH_period_h||8;
  $('#f_aq_man').checked=!!c.autoQNH_manual_en;
  $('#f_aq_icao').value=(c.autoQNH_manual_icao||'').toUpperCase();
  updateAutoQnhInputs();
  $('#saveHint').textContent='Změny zatím neuloženy.';
}

async function saveConfig(persist){
  const body={
    deviceName:$('#f_name').value.trim(),
    unitId:+$('#f_unit').value,
    pollMs:+$('#f_poll').value,
    autoTest:$('#f_autotest').checked?1:0,
    cfgElevation_m:+$('#f_elev').value,
    cfgQNH_Pa:+$('#f_qnh').value,
    altMode:+$('#f_altmode').value,
    autoQNH_enable:$('#f_aq_en').checked?1:0,
    autoQNH_period_h:+$('#f_aq_per').value,
    autoQNH_manual_en:$('#f_aq_man').checked?1:0,
    autoQNH_manual_icao:$('#f_aq_icao').value.trim().toUpperCase(),
    persist:!!persist
  };
  const r=await api('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  $('#saveHint').textContent=r.msg||(persist?'Uloženo.':'Aplikováno.');
  if(r.reboot)setTimeout(()=>location.reload(),1200);
  refresh();
}

async function action(cmd){return api('/api/action',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cmd})});}

async function doScan(){
  $('#i2cTable').innerHTML='<tr><td colspan="2" class="muted">Probíhá…</td></tr>';
  await action('scan');
  const st=await api('/api/status');
  const list=(st.i2c||[]);
  if(!list.length){$('#i2cTable').innerHTML='<tr><td colspan="2">Nic nenalezeno</td></tr>';return;}
  $('#i2cTable').innerHTML=list.map(v=>`<tr><td>${v}</td><td class="mono">0x${(+v).toString(16).padStart(2,'0')}</td></tr>`).join('');
}

async function doSelfTest(){
  $('#stHint').textContent='Běží…';
  await action('selftest');
  const st=await api('/api/status');
  $('#stHint').textContent=`HTU=${st.htu_ok?'OK':'NOK'} • BH1750=${st.bh_ok?'OK':'NOK'} • BMP=${st.bmp_ok?'OK':'NOK'}`;
}

async function doAqRun(){
  $('#aqRunHint').textContent='Běží…';
  await action('aq_run');
  const st=await api('/api/status');
  $('#aqRunHint').textContent=`Výsledek=${fmt.aqResult(st.aq_last_result)} • QNH=${fmt.pa(st.qnh)}`;
}

async function mbRead(){
  const addr=+$('#mb_addr').value, n=+$('#mb_n').value;
  const r=await api(`/api/mb?addr=${addr}&n=${n}`);
  $('#mbTable').innerHTML=r.length?r.map((v,i)=>`<tr><td>${addr+i}</td><td>${v}</td></tr>`).join(''):'<tr><td colspan="2" class="muted">Žádná data</td></tr>';
}

async function mbWrite(){
  const addr=+$('#mb_addr').value; const val=$('#mb_w_val').value;
  await api('/api/mb',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({addr,val:+val})});
  mbRead();
}

function initTabs(){
  const tabs=['dash','charts','settings','tools','modbus','features'];
  $$('.tabs button').forEach(btn=>{
    btn.onclick=()=>{
      $$('.tabs button').forEach(b=>b.classList.remove('active'));
      btn.classList.add('active');
      tabs.forEach(t=>{$(`#tab-${t}`).style.display=btn.dataset.tab===t?'block':'none';});
      if(btn.dataset.tab==='charts')drawChart();
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
  $('#f_aq_en').addEventListener('change',updateAutoQnhInputs);
  $('#f_aq_man').addEventListener('change',updateAutoQnhInputs);

  await refresh();
  await loadConfig();
  setInterval(refresh,4000);
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

  StaticJsonDocument<896> doc;
  doc["deviceName"] = CFG.deviceName;
  doc["fw_major"] = FW_MAJOR;
  doc["fw_minor"] = FW_MINOR;
  doc["wifi_ok"] = wifiConnected;
  doc["ip"] = wifiConnected ? ip.toString() : String("0.0.0.0");
  doc["rssi"] = WiFi.RSSI();
  doc["unitId"] = CFG.unitId;
  doc["uptime_s"] = (millis() - bootMillis) / 1000UL;
  doc["altMode"] = (unsigned)mb.Hreg(39);
  doc["pollMs"] = CFG.pollMs;
  doc["autoTest"] = CFG.autoTest;
  doc["cfgElevation_m"] = (int)CFG.cfgElevation_m;
  doc["cfgQNH_Pa"] = (unsigned long)CFG.cfgQNH_Pa;
  doc["autoQNH_enable"] = (unsigned)CFG.autoQNH_enable;
  doc["autoQNH_period_h"] = (unsigned)CFG.autoQNH_period_h;
  doc["autoQNH_manual_en"] = (unsigned)CFG.autoQNH_manual_en;
  doc["autoQNH_manual_icao"] = CFG.autoQNH_manual_icao;

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
