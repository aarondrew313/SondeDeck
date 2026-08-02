#include "web_dashboard.h"

#include <WiFi.h>
#include <math.h>

#include "../config/version.h"

namespace {
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SondeDeck Web</title>
<style>
:root{--bg:#050805;--panel:#101510;--panel2:#0b100b;--text:#f2f2f2;--muted:#8b958b;--green:#00ff66;--amber:#ffb400;--red:#ff3c3c;--line:#263026}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial,sans-serif}.wrap{max-width:760px;margin:0 auto;padding:16px}.brand{display:flex;align-items:baseline;gap:10px;border-bottom:1px solid var(--line);padding-bottom:12px;margin-bottom:14px}.brand h1{font-size:28px;line-height:1;margin:0;color:var(--green);letter-spacing:.3px}.brand .ver{color:var(--muted);font-size:14px}.status{display:grid;grid-template-columns:repeat(6,1fr);gap:7px;margin-bottom:14px}.pill{background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:8px 6px;text-align:center;font-weight:700;font-size:13px}.ok{color:var(--green);border-color:#116b35}.warn{color:var(--amber);border-color:#6b5011}.bad{color:var(--red);border-color:#6b1b1b}.hero{background:linear-gradient(180deg,#102015,#091009);border:1px solid #164c2b;border-radius:14px;padding:18px;margin-bottom:14px}.hero .state{font-size:30px;font-weight:800;color:var(--green);letter-spacing:.5px}.hero .sub{color:var(--muted);margin-top:4px}.grid{display:grid;grid-template-columns:repeat(2,1fr);gap:12px}.card{background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:14px}.card h2{font-size:15px;margin:0 0 10px;color:var(--green);font-weight:800}.row{display:flex;justify-content:space-between;gap:12px;border-top:1px solid #151d15;padding:7px 0;font-size:14px}.row:first-of-type{border-top:0}.k{color:var(--muted)}.v{text-align:right;font-variant-numeric:tabular-nums}.mapbtn{display:block;margin-top:10px;text-align:center;text-decoration:none;border:1px solid #116b35;border-radius:10px;padding:10px;color:var(--green);font-weight:800;background:#081208}.mapbtn.disabled{color:var(--muted);border-color:var(--line);pointer-events:none}.footer{color:var(--muted);font-size:12px;text-align:center;margin:18px 0 4px}@media(max-width:620px){.status{grid-template-columns:repeat(3,1fr)}.grid{grid-template-columns:1fr}.hero .state{font-size:26px}}
</style>
</head>
<body>
<div class="wrap">
  <div class="brand"><h1>SondeDeck</h1><div class="ver" id="version">--</div></div>
  <div class="status">
    <div class="pill" id="gps">G--</div><div class="pill" id="sonde">S--</div><div class="pill" id="recovery">R--</div>
    <div class="pill" id="logging">SD</div><div class="pill" id="freq">F--</div><div class="pill" id="online">ONL</div>
  </div>
  <div class="hero"><div class="state" id="headline">WAITING</div><div class="sub" id="headlineSub">No status yet</div></div>
  <div class="grid">
    <div class="card"><h2>Sonde</h2><div id="sondeRows"></div></div>
    <div class="card"><h2>Recovery</h2><div id="recoveryRows"></div></div>
    <div class="card"><h2>Local GPS</h2><div id="localRows"></div></div>
    <div class="card"><h2>System</h2><div id="systemRows"></div></div>
  </div>
  <div class="footer">Read-only live dashboard. SondeDeck controls remain on the device.</div>
</div>
<script>
function cls(el,s){el.className='pill '+s}
function row(k,v){return `<div class="row"><div class="k">${k}</div><div class="v">${v??'--'}</div></div>`}
function num(v,d=1,s=''){return Number.isFinite(v)?v.toFixed(d)+s:'--'}
function dist(m){return Number.isFinite(m)?(m<1000?m.toFixed(0)+' m':(m/1000).toFixed(2)+' km'):'--'}
function age(ms){return Number.isFinite(ms)?(ms/1000).toFixed(0)+'s ago':'--'}
function setPill(id,val,state){const e=document.getElementById(id);e.textContent=val;cls(e,state)}
function mapsLink(lat,lon){return `https://www.google.com/maps/search/?api=1&query=${lat},${lon}`}
async function refresh(){try{const r=await fetch('/api/status',{cache:'no-store'});const d=await r.json();
 document.getElementById('version').textContent=d.version||'';
 setPill('gps',d.status.gps,d.local.fix?'ok':(d.local.chars>0?'warn':'bad'));
 setPill('sonde',d.status.sonde,d.sonde.gps?'ok':(d.sonde.heard?'warn':'bad'));
 setPill('recovery',d.status.recovery,d.recovery.ready?'ok':(d.local.fix||d.sonde.gps?'warn':'bad'));
 setPill('logging',d.status.logging,d.logging.enabled?'ok':(d.logging.available?'warn':'bad'));
 setPill('freq',d.status.frequency,d.frequency.locked?'ok':(d.frequency.scan?'warn':'bad'));
 setPill('online',d.status.online,d.network.connected?'ok':(d.network.configured?'warn':'bad'));
 const h=document.getElementById('headline'), hs=document.getElementById('headlineSub');
 if(d.sonde.heard){h.textContent='SONDE HEARD';h.style.color='var(--green)';hs.textContent=`${d.sonde.serial||'RS41'}  RSSI ${d.sonde.rssi_dbm} dBm  last frame ${age(d.sonde.last_seen_ms)}`}
 else{h.textContent='WAITING FOR SONDE';h.style.color='var(--amber)';hs.textContent=`Listening on ${num(d.frequency.mhz,3,' MHz')}`}
 document.getElementById('sondeRows').innerHTML=row('Serial',d.sonde.serial)+row('Frame',d.sonde.frame)+row('RSSI',d.sonde.rssi_dbm+' dBm')+row('Peak',d.sonde.peak_rssi_dbm+' dBm')+row('GPS',d.sonde.gps?'valid':'waiting')+row('Lat/Lon',d.sonde.gps?`${num(d.sonde.lat,6)}, ${num(d.sonde.lon,6)}`:'--')+row('Alt',num(d.sonde.alt_m,1,' m'))+row('Sats',d.sonde.sats)+(d.sonde.gps?`<a class="mapbtn" href="${mapsLink(d.sonde.lat,d.sonde.lon)}" target="_blank" rel="noopener">Open sonde in Google Maps</a>`:`<a class="mapbtn disabled">Google Maps waits for sonde GPS</a>`);
 document.getElementById('recoveryRows').innerHTML=row('Ready',d.recovery.ready?'yes':'no')+row('Range',dist(d.recovery.range_m))+row('Bearing',num(d.recovery.bearing_deg,0,'°'))+row('Elevation',num(d.recovery.elevation_deg,0,'°'))+row('Line distance',dist(d.recovery.line_m));
 document.getElementById('localRows').innerHTML=row('Fix',d.local.fix?'yes':'no')+row('Sats',d.local.sats)+row('HDOP',num(d.local.hdop,1))+row('Lat/Lon',d.local.fix?`${num(d.local.lat,6)}, ${num(d.local.lon,6)}`:'--')+row('Alt',num(d.local.alt_m,1,' m'));
 document.getElementById('systemRows').innerHTML=row('Battery',`${d.battery.percent}%  ${num(d.battery.voltage,2,' V')}`)+row('Frequency',num(d.frequency.mhz,3,' MHz'))+row('Scan',d.frequency.scan?'on':'off')+row('Logging',d.logging.enabled?'enabled':(d.logging.available?'off':'unavailable'))+row('Wi-Fi',d.network.status)+row('IP',d.network.ip||'--');
}catch(e){document.getElementById('headline').textContent='CONNECTION LOST';document.getElementById('headline').style.color='var(--red)';document.getElementById('headlineSub').textContent='Waiting for SondeDeck...'} }
refresh();setInterval(refresh,1000);
</script>
</body></html>
)rawliteral";

void appendStatusCode(String& output, const char* key, const char* value) {
    output += "\"";
    output += key;
    output += "\":\"";
    output += value;
    output += "\"";
}
}

bool WebDashboard::begin() {
    if (WiFi.status() != WL_CONNECTED) {
        strncpy(statusText_, "waiting for Wi-Fi", sizeof(statusText_) - 1);
        statusText_[sizeof(statusText_) - 1] = '\0';
        ipAddress_[0] = '-';
        ipAddress_[1] = '-';
        ipAddress_[2] = '\0';
        return false;
    }

    if (!routesConfigured_) {
        server_.on("/", HTTP_GET, [this]() { handleRoot(); });
        server_.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
        server_.onNotFound([this]() { handleNotFound(); });
        routesConfigured_ = true;
    }

    server_.begin();
    running_ = true;

    const String ip = WiFi.localIP().toString();
    strncpy(ipAddress_, ip.c_str(), sizeof(ipAddress_) - 1);
    ipAddress_[sizeof(ipAddress_) - 1] = '\0';

    snprintf(statusText_, sizeof(statusText_), "http://%s/", ipAddress_);

    return true;
}

void WebDashboard::stop() {
    if (running_) {
        server_.stop();
    }

    running_ = false;
    strncpy(statusText_, "off", sizeof(statusText_) - 1);
    statusText_[sizeof(statusText_) - 1] = '\0';
}

void WebDashboard::handleClient() {
    if (!running_) {
        return;
    }

    server_.handleClient();
}

bool WebDashboard::running() const {
    return running_;
}

const char* WebDashboard::ipAddress() const {
    return ipAddress_;
}

const char* WebDashboard::statusText() const {
    return statusText_;
}

void WebDashboard::appendJsonString(String& output, const char* value) {
    output += '"';

    if (value != nullptr) {
        for (const char* c = value; *c != '\0'; ++c) {
            switch (*c) {
                case '"':
                    output += "\\\"";
                    break;
                case '\\':
                    output += "\\\\";
                    break;
                case '\n':
                    output += "\\n";
                    break;
                case '\r':
                    output += "\\r";
                    break;
                case '\t':
                    output += "\\t";
                    break;
                default:
                    if (static_cast<uint8_t>(*c) >= 32) {
                        output += *c;
                    }
                    break;
            }
        }
    }

    output += '"';
}

void WebDashboard::appendJsonNumber(String& output, double value, uint8_t decimals) {
    if (!isfinite(value)) {
        output += "null";
        return;
    }

    char buffer[32];
    snprintf(
        buffer,
        sizeof(buffer),
        "%.*f",
        static_cast<int>(decimals),
        value
    );
    output += buffer;
}

void WebDashboard::updateSnapshot(
    const SondeTelemetry* telemetry,
    bool hasTelemetry,
    bool sondeGpsUsable,
    const NavigationInfo& navigation,
    const AppStatus& status
) {
    const bool heard = hasTelemetry && telemetry != nullptr;
    const bool recoveryReady = navigation.localFixValid && sondeGpsUsable;
    const bool frequencyLocked = status.frequency.locked || heard;

    char gpsCode[8];
    if (navigation.localFixValid) {
        snprintf(gpsCode, sizeof(gpsCode), "G%02u", navigation.localSatellites);
    } else if (status.localGpsChars > 0) {
        snprintf(gpsCode, sizeof(gpsCode), "G..");
    } else {
        snprintf(gpsCode, sizeof(gpsCode), "G--");
    }

    const char* sondeCode = sondeGpsUsable ? "S+" : (heard ? "S?" : "S--");
    const char* recoveryCode = recoveryReady ? "R+" : ((navigation.localFixValid || sondeGpsUsable) ? "R?" : "R--");
    const char* loggingCode = status.logger.enabled ? "LOG" : (status.logger.available ? "SD" : "SD!");
    const char* frequencyCode = frequencyLocked ? "LCK" : (status.frequency.scanEnabled ? "SCN" : "F--");
    const char* onlineCode = status.network.connected ? "ONL" : (status.network.configured ? "W.." : "W--");

    String json;
    json.reserve(2400);

    json += "{";
    json += "\"version\":";
    appendJsonString(json, VersionInfo::VERSION);

    json += ",\"status\":{";
    appendStatusCode(json, "gps", gpsCode);
    json += ',';
    appendStatusCode(json, "sonde", sondeCode);
    json += ',';
    appendStatusCode(json, "recovery", recoveryCode);
    json += ',';
    appendStatusCode(json, "logging", loggingCode);
    json += ',';
    appendStatusCode(json, "frequency", frequencyCode);
    json += ',';
    appendStatusCode(json, "online", onlineCode);
    json += '}';

    json += ",\"sonde\":{";
    json += "\"heard\":";
    json += heard ? "true" : "false";
    json += ",\"gps\":";
    json += sondeGpsUsable ? "true" : "false";
    json += ",\"serial\":";
    appendJsonString(json, heard ? telemetry->serial : "");
    json += ",\"frame\":";
    json += heard ? String(telemetry->frameNumber) : "null";
    json += ",\"rssi_dbm\":";
    json += heard ? String(static_cast<int>(telemetry->rssiDbm)) : "null";
    json += ",\"peak_rssi_dbm\":";
    json += String(static_cast<int>(status.counters.peakRssiDbm));
    json += ",\"last_seen_ms\":";
    json += heard ? String(status.msSinceLastValidFrame) : "null";
    json += ",\"lat\":";
    appendJsonNumber(json, heard && sondeGpsUsable ? telemetry->latitude : NAN, 6);
    json += ",\"lon\":";
    appendJsonNumber(json, heard && sondeGpsUsable ? telemetry->longitude : NAN, 6);
    json += ",\"alt_m\":";
    appendJsonNumber(json, heard && sondeGpsUsable ? telemetry->altitudeMetres : NAN, 1);
    json += ",\"sats\":";
    json += heard ? String(telemetry->satellites) : "null";
    json += '}';

    json += ",\"local\":{";
    json += "\"fix\":";
    json += navigation.localFixValid ? "true" : "false";
    json += ",\"chars\":";
    json += String(status.localGpsChars);
    json += ",\"sats\":";
    json += String(navigation.localSatellites);
    json += ",\"hdop\":";
    appendJsonNumber(json, navigation.localHdop, 1);
    json += ",\"lat\":";
    appendJsonNumber(json, navigation.localFixValid ? navigation.localLatitude : NAN, 6);
    json += ",\"lon\":";
    appendJsonNumber(json, navigation.localFixValid ? navigation.localLongitude : NAN, 6);
    json += ",\"alt_m\":";
    appendJsonNumber(json, navigation.localFixValid ? navigation.localAltitudeMetres : NAN, 1);
    json += '}';

    json += ",\"recovery\":{";
    json += "\"ready\":";
    json += recoveryReady ? "true" : "false";
    json += ",\"range_m\":";
    appendJsonNumber(json, navigation.navValid ? navigation.distanceMetres : NAN, 1);
    json += ",\"bearing_deg\":";
    appendJsonNumber(json, navigation.navValid ? navigation.bearingDegrees : NAN, 1);
    json += ",\"elevation_deg\":";
    appendJsonNumber(json, navigation.navValid ? navigation.elevationDegrees : NAN, 1);
    json += ",\"line_m\":";
    appendJsonNumber(json, navigation.navValid ? navigation.straightLineMetres : NAN, 1);
    json += '}';

    json += ",\"battery\":{";
    json += "\"percent\":";
    json += String(status.battery.percent);
    json += ",\"voltage\":";
    appendJsonNumber(json, status.battery.voltage, 2);
    json += ",\"external\":";
    json += status.battery.externalPowerLikely ? "true" : "false";
    json += '}';

    json += ",\"frequency\":{";
    json += "\"mhz\":";
    appendJsonNumber(json, status.frequency.currentFrequencyHz / 1000000.0, 3);
    json += ",\"scan\":";
    json += status.frequency.scanEnabled ? "true" : "false";
    json += ",\"locked\":";
    json += frequencyLocked ? "true" : "false";
    json += '}';

    json += ",\"logging\":{";
    json += "\"available\":";
    json += status.logger.available ? "true" : "false";
    json += ",\"enabled\":";
    json += status.logger.enabled ? "true" : "false";
    json += ",\"frames\":";
    json += String(status.logger.framesLogged);
    json += '}';

    json += ",\"network\":{";
    json += "\"configured\":";
    json += status.network.configured ? "true" : "false";
    json += ",\"connected\":";
    json += status.network.connected ? "true" : "false";
    json += ",\"status\":";
    appendJsonString(json, status.network.status);
    json += ",\"ssid\":";
    appendJsonString(json, status.network.ssid);
    json += ",\"ip\":";
    appendJsonString(json, status.network.ipAddress);
    json += '}';

    json += '}';

    cachedJson_ = json;
}

void WebDashboard::handleRoot() {
    server_.send_P(200, "text/html", INDEX_HTML);
}

void WebDashboard::handleStatus() {
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(200, "application/json", cachedJson_);
}

void WebDashboard::handleNotFound() {
    server_.send(404, "text/plain", "Not found");
}
