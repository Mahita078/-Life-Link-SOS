/*
 * ============================================================
 *  ESP8266 LoRa Receiver — Web Dashboard + I2C LCD + Callback
 * ============================================================
 *  Board   : NodeMCU (ESP8266)
 *  LoRa    : SX1278 / Ra-02 @ 433 MHz
 *  LCD     : 16x2 I2C (PCF8574, address 0x27)
 *
 *  WIRING
 *  ------
 *  LoRa SS    -> D8  (GPIO15)
 *  LoRa RST   -> D0  (GPIO16)
 *  LoRa DIO0  -> D3  (GPIO0)
 *  LoRa SCK   -> D5  (GPIO14)
 *  LoRa MISO  -> D6  (GPIO12)
 *  LoRa MOSI  -> D7  (GPIO13)
 *  LoRa VCC   -> 3.3V  (NEVER 5V)
 *  LoRa GND   -> GND
 *
 *  LCD SDA    -> D2  (GPIO4)
 *  LCD SCL    -> D1  (GPIO5)
 *  LCD VCC    -> VIN or 3.3V
 *  LCD GND    -> GND
 *
 *  WiFi SSID  : "lora"
 *  WiFi Pass  : "123456789"
 * ============================================================
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---------------- WiFi ----------------
const char* ssid     = "lora";
const char* password = "123456789";

// ---------------- LoRa pins (NodeMCU) ----------------
#define LORA_SS    15   // D8
#define LORA_RST   16   // D0
#define LORA_DIO0   0   // D3  (GPIO0)

// ---------------- Objects ----------------
ESP8266WebServer    server(80);
LiquidCrystal_I2C   lcd(0x27, 16, 2);

// ---------------- State ----------------
float lastTemp  = 0.0f;
float lastHum   = 0.0f;
int   lastRSSI  = 0;
float lastSNR   = 0.0f;
unsigned long lastRx   = 0;
int  packetCount = 0;

String packetLog[10];
int    packetRSSI[10];
int    logIndex = 0;

// =====================================================================
//  HTML — professional dark dashboard with routing tabs
// =====================================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>LoRa Receiver • Dashboard</title>
<style>
:root{
  --bg:#0b1220; --card:#111a2e; --line:#1e2a44;
  --txt:#e5edf7; --muted:#7d8aa3; --accent:#38bdf8;
  --ok:#22c55e; --warn:#f59e0b; --err:#ef4444;
  --temp:#f87171; --hum:#2dd4bf;
}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',system-ui,Arial,sans-serif;background:var(--bg);color:var(--txt);min-height:100vh}
header{background:linear-gradient(90deg,#0f172a,#111a2e);border-bottom:1px solid var(--line);
  padding:14px 22px;display:flex;align-items:center;justify-content:space-between;
  position:sticky;top:0;z-index:10;backdrop-filter:blur(8px);flex-wrap:wrap;gap:10px}
.brand{display:flex;align-items:center;gap:10px;font-weight:600;letter-spacing:.3px}
.dot{width:10px;height:10px;border-radius:50%;background:var(--ok);
  box-shadow:0 0 10px var(--ok);animation:pulse 2s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.35}}
nav{display:flex;gap:6px;flex-wrap:wrap}
nav button{background:transparent;border:1px solid transparent;color:var(--muted);
  padding:8px 14px;border-radius:10px;cursor:pointer;font-size:14px;transition:.2s}
nav button:hover{color:var(--txt);background:#0e1626}
nav button.active{color:var(--accent);background:#0e1626;border-color:var(--line)}
main{max-width:1100px;margin:0 auto;padding:24px 18px}
.page{display:none;animation:fade .25s ease}
.page.active{display:block}
@keyframes fade{from{opacity:0;transform:translateY(6px)}to{opacity:1;transform:none}}
h1{font-size:20px;font-weight:600;margin-bottom:4px}
.sub{color:var(--muted);font-size:13px;margin-bottom:20px}
.grid{display:grid;gap:16px}
.g2{grid-template-columns:repeat(auto-fit,minmax(260px,1fr))}
.g3{grid-template-columns:repeat(auto-fit,minmax(220px,1fr))}
.card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:20px}
.card h3{font-size:13px;font-weight:500;color:var(--muted);
  text-transform:uppercase;letter-spacing:.08em;margin-bottom:8px}
.big{font-size:52px;font-weight:700;line-height:1.1}
.big.t{color:var(--temp)} .big.h{color:var(--hum)}
.unit{font-size:14px;color:var(--muted);font-weight:400}
.meta{display:flex;justify-content:space-between;margin-top:14px;
  padding-top:12px;border-top:1px solid var(--line);font-size:12px;color:var(--muted)}
table{width:100%;border-collapse:collapse;font-size:13px}
th,td{padding:10px 8px;text-align:left;border-bottom:1px solid var(--line)}
th{color:var(--muted);font-weight:500;text-transform:uppercase;font-size:11px;letter-spacing:.06em}
tr:hover td{background:#0e1626}
.pill{display:inline-block;padding:2px 9px;border-radius:999px;font-size:11px;font-weight:600}
.pill.ok{background:rgba(34,197,94,.15);color:var(--ok)}
.pill.warn{background:rgba(245,158,11,.15);color:var(--warn)}
.pill.err{background:rgba(239,68,68,.15);color:var(--err)}
.mono{font-family:'JetBrains Mono',Consolas,monospace;font-size:12px}
.kv{display:flex;justify-content:space-between;padding:9px 0;
  border-bottom:1px dashed var(--line);font-size:13px}
.kv:last-child{border-bottom:0}
.kv span:first-child{color:var(--muted)}
.kv span:last-child{font-weight:600}
footer{text-align:center;color:var(--muted);font-size:12px;padding:24px}
</style>
</head>
<body>
<header>
  <div class="brand"><span class="dot" id="live"></span> LoRa Receiver • 433 MHz</div>
  <nav>
    <button data-tab="dash" class="active">Dashboard</button>
    <button data-tab="pkts">Packets</button>
    <button data-tab="sys">System</button>
  </nav>
</header>

<main>
  <section id="dash" class="page active">
    <h1>Live Telemetry</h1>
    <p class="sub">Real-time sensor data received over LoRa</p>
    <div class="grid g2">
      <div class="card">
        <h3>Temperature</h3>
        <div class="big t"><span id="temp">--</span><span class="unit"> °C</span></div>
        <div class="meta"><span>Sensor: DHT11</span><span id="tempAgo">—</span></div>
      </div>
      <div class="card">
        <h3>Humidity</h3>
        <div class="big h"><span id="hum">--</span><span class="unit"> %</span></div>
        <div class="meta"><span>Sensor: DHT11</span><span id="humAgo">—</span></div>
      </div>
    </div>
    <div class="grid g3" style="margin-top:16px">
      <div class="card"><h3>RSSI</h3>
        <div class="big mono" id="rssi" style="font-size:32px">--</div>
        <div class="meta"><span>Signal</span><span id="rssiLbl">—</span></div>
      </div>
      <div class="card"><h3>Packets</h3>
        <div class="big mono" id="count" style="font-size:32px">0</div>
        <div class="meta"><span>Received</span><span id="rate">0 / min</span></div>
      </div>
      <div class="card"><h3>Last Packet</h3>
        <div class="big mono" id="age" style="font-size:32px">--</div>
        <div class="meta"><span>Ago (s)</span><span id="status"><span class="pill warn">waiting</span></span></div>
      </div>
    </div>
  </section>

  <section id="pkts" class="page">
    <h1>Packet Log</h1>
    <p class="sub">Last 10 raw packets received</p>
    <div class="card">
      <table>
        <thead><tr><th>#</th><th>Payload</th><th>RSSI</th></tr></thead>
        <tbody id="logBody">
          <tr><td colspan="3" style="color:#7d8aa3">No packets yet…</td></tr>
        </tbody>
      </table>
    </div>
  </section>

  <section id="sys" class="page">
    <h1>System</h1>
    <p class="sub">Device & network information</p>
    <div class="grid g2">
      <div class="card">
        <h3>Device</h3>
        <div class="kv"><span>IP Address</span><span class="mono" id="ip">—</span></div>
        <div class="kv"><span>Uptime</span><span id="uptime">—</span></div>
        <div class="kv"><span>Free Heap</span><span id="heap">—</span></div>
        <div class="kv"><span>WiFi RSSI</span><span id="wifiRssi">—</span></div>
      </div>
      <div class="card">
        <h3>LoRa</h3>
        <div class="kv"><span>Frequency</span><span>433 MHz</span></div>
        <div class="kv"><span>Mode</span><span>Receive (callback)</span></div>
        <div class="kv"><span>Last RSSI</span><span id="loraRssi">—</span></div>
        <div class="kv"><span>Last SNR</span><span id="loraSnr">—</span></div>
      </div>
    </div>
  </section>
</main>
<footer>LoRa Weather Station • ESP8266 • 433 MHz</footer>

<script>
document.querySelectorAll('nav button').forEach(b=>{
  b.onclick=()=>{
    document.querySelectorAll('nav button').forEach(x=>x.classList.remove('active'));
    document.querySelectorAll('.page').forEach(x=>x.classList.remove('active'));
    b.classList.add('active');
    document.getElementById(b.dataset.tab).classList.add('active');
  };
});
function fmtAgo(s){
  if(s<0) return '—';
  if(s<60) return s+'s ago';
  if(s<3600) return Math.floor(s/60)+'m ago';
  return Math.floor(s/3600)+'h ago';
}
function fmtUp(sec){
  const d=Math.floor(sec/86400), h=Math.floor(sec%86400/3600),
        m=Math.floor(sec%3600/60), s=sec%60;
  return (d?d+'d ':'')+(h?h+'h ':'')+(m?m+'m ':'')+s+'s';
}
let rateWin=[];
async function refresh(){
  try{
    const r=await fetch('/data');
    const d=await r.json();
    document.getElementById('temp').textContent=d.temp.toFixed(1);
    document.getElementById('hum').textContent =d.hum.toFixed(1);
    document.getElementById('rssi').textContent=d.rssi;
    document.getElementById('rssiLbl').textContent =
       d.rssi>-70?'Excellent':d.rssi>-90?'Good':'Weak';
    document.getElementById('count').textContent=d.count;
    document.getElementById('age').textContent  =d.age;
    document.getElementById('tempAgo').textContent=fmtAgo(d.age);
    document.getElementById('humAgo').textContent =fmtAgo(d.age);

    const st=document.getElementById('status');
    if(d.age<10)      st.innerHTML='<span class="pill ok">live</span>';
    else if(d.age<60) st.innerHTML='<span class="pill warn">delayed</span>';
    else              st.innerHTML='<span class="pill err">stale</span>';

    const now=Date.now();
    rateWin.push({t:now,c:d.count});
    rateWin=rateWin.filter(x=>now-x.t<60000);
    const f=rateWin[0];
    if(f&&now-f.t>5000){
      document.getElementById('rate').textContent =
        (((d.count-f.c)/((now-f.t)/1000))*60).toFixed(1)+' / min';
    }

    const tb=document.getElementById('logBody');
    const rows=[];
    for(let i=0;i<d.log.length;i++){
      const p=d.log[i];
      if(!p) continue;
      rows.push(`<tr><td>${d.count-i}</td><td class="mono">${p.raw}</td><td class="mono">${p.rssi} dBm</td></tr>`);
    }
    tb.innerHTML = rows.length? rows.join('') :
      '<tr><td colspan="3" style="color:#7d8aa3">No packets yet…</td></tr>';

    document.getElementById('ip').textContent      =d.ip;
    document.getElementById('uptime').textContent  =fmtUp(d.uptime);
    document.getElementById('heap').textContent    =d.heap+' B';
    document.getElementById('wifiRssi').textContent=d.wifiRssi+' dBm';
    document.getElementById('loraRssi').textContent=d.rssi+' dBm';
    document.getElementById('loraSnr').textContent =d.snr.toFixed(1)+' dB';
    document.getElementById('live').style.background='#22c55e';
  }catch(e){
    document.getElementById('live').style.background='#ef4444';
  }
}
setInterval(refresh,1000); refresh();
</script>
</body>
</html>
)rawliteral";

// =====================================================================
//  Web handlers
// =====================================================================
void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleData() {
  unsigned long age = lastRx ? (millis() - lastRx) / 1000 : 0;

  // Build log JSON — newest first
  String logJson = "[";
  for (int i = 0; i < 10; i++) {
    int idx = (logIndex - 1 - i + 100) % 10;
    if (i) logJson += ",";
    if (packetLog[idx].length()) {
      String raw = packetLog[idx];
      raw.replace("\"", "'");
      raw.replace("\\", "/");
      raw.replace("\n", " ");
      raw.replace("\r", " ");
      logJson += "{\"raw\":\"" + raw + "\",\"rssi\":" + String(packetRSSI[idx]) + "}";
    } else {
      logJson += "null";
    }
  }
  logJson += "]";

  String json = "{";
  json += "\"temp\":"    + String(lastTemp, 1) + ",";
  json += "\"hum\":"     + String(lastHum, 1)  + ",";
  json += "\"rssi\":"    + String(lastRSSI)    + ",";
  json += "\"snr\":"     + String(lastSNR, 1)  + ",";
  json += "\"count\":"   + String(packetCount) + ",";
  json += "\"age\":"     + String(age)         + ",";
  json += "\"uptime\":"  + String(millis() / 1000) + ",";
  json += "\"heap\":"    + String(ESP.getFreeHeap()) + ",";
  json += "\"ip\":\""    + WiFi.localIP().toString() + "\",";
  json += "\"wifiRssi\":"+ String(WiFi.RSSI()) + ",";
  json += "\"log\":"     + logJson;
  json += "}";
  server.send(200, "application/json", json);
}

// =====================================================================
//  LCD
// =====================================================================
void updateLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:");  lcd.print(lastTemp, 1); lcd.print((char)223); lcd.print("C");
  lcd.print(" H:"); lcd.print(lastHum, 1);  lcd.print("%");

  lcd.setCursor(0, 1);
  lcd.print("Pkt:"); lcd.print(packetCount);
  lcd.print(" ");
  if (WiFi.status() == WL_CONNECTED) {
    lcd.print(WiFi.localIP().toString());
  } else {
    lcd.print("No WiFi");
  }
}

// =====================================================================
//  Parser helper — returns the number that follows a label
//  e.g. valueAfterLabel("Temp: 35.60C", "Temp")  -> 35.60
// =====================================================================
float valueAfterLabel(const String &s, const String &label) {
  int p = s.indexOf(label);
  if (p < 0) return NAN;
  p += label.length();
  // skip spaces / ':' / any non-numeric until digit, '-' or '.'
  while (p < (int)s.length() &&
         !isDigit(s[p]) && s[p] != '-' && s[p] != '.') p++;
  int start = p;
  while (p < (int)s.length() &&
         (isDigit(s[p]) || s[p] == '.' || s[p] == '-')) p++;
  if (p == start) return NAN;
  return s.substring(start, p).toFloat();
}

// =====================================================================
//  LoRa receive callback
// =====================================================================
void onReceive(int packetSize) {
  if (packetSize == 0) return;

  String incoming = "";
  for (int i = 0; i < packetSize; i++) incoming += (char)LoRa.read();

  lastRSSI = LoRa.packetRssi();
  lastSNR  = LoRa.packetSnr();
  packetCount++;
  lastRx = millis();

  // store in ring buffer
  packetLog[logIndex]  = incoming;
  packetRSSI[logIndex] = lastRSSI;
  logIndex = (logIndex + 1) % 10;

  // ---- Parse ----
  float t = NAN, h = NAN;

  // 1) Preferred: labeled format  "Temp: 35.60 ... Hum: 56.70"
  if (incoming.indexOf("Temp") >= 0 || incoming.indexOf("temp") >= 0) {
    t = valueAfterLabel(incoming, "Temp");
    if (isnan(t)) t = valueAfterLabel(incoming, "temp");
    h = valueAfterLabel(incoming, "Hum");
    if (isnan(h)) h = valueAfterLabel(incoming, "hum");
  }
  // 2) Fallback: plain "35.60,56.70"
  else {
    int comma = incoming.indexOf(',');
    if (comma > 0) {
      t = incoming.substring(0, comma).toFloat();
      h = incoming.substring(comma + 1).toFloat();
    }
  }

  if (!isnan(t) && !isnan(h)) {
    lastTemp = t;
    lastHum  = h;
    updateLCD();
  }

  Serial.printf("RX #%d: '%s'  RSSI=%d  SNR=%.1f  -> T=%.2f H=%.2f\n",
                packetCount, incoming.c_str(), lastRSSI, lastSNR, t, h);
}

// =====================================================================
//  setup / loop
// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== LoRa Receiver (Web + LCD + Callback) ===");

  // ---- LCD ----
  Wire.begin(D2, D1);   // SDA=D2, SCL=D1
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("RX Starting...");

  // ---- LoRa ----
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed!");
    lcd.clear();
    lcd.print("LoRa FAIL");
    while (1) delay(1000);
  }
  LoRa.onReceive(onReceive);
  LoRa.receive();                 // enter RX mode (callback-driven)
  Serial.println("LoRa OK @ 433 MHz (callback mode)");

  // ---- WiFi ----
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(400); Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nIP: " + WiFi.localIP().toString());
    server.on("/",     handleRoot);
    server.on("/data", handleData);
    server.begin();
    Serial.println("Web server started");
  } else {
    Serial.println("\nWiFi FAILED");
  }

  // ---- Ready screen ----
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("RX Ready");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.status() == WL_CONNECTED
              ? WiFi.localIP().toString()
              : "No WiFi");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) server.handleClient();
}