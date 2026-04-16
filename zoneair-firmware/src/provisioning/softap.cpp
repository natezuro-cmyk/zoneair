#include "softap.h"

#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>

namespace zoneair {

static SoftApProvisioner::OnProvisioned g_cb;
static DNSServer g_dns;
static AsyncWebServer g_ap_server(80);

// Minimal captive-portal HTML (works on iOS, Android, macOS, Windows).
static const char* PAGE_HTML = R"HTML(<!doctype html>
<html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Z1 Air Setup</title>
<style>
  body{font-family:-apple-system,system-ui,sans-serif;background:#0a0e14;color:#e6edf3;
       margin:0;padding:32px 20px;min-height:100vh;box-sizing:border-box}
  h1{font-weight:500;font-size:20px;margin:0 0 4px}
  .sub{color:#7d8896;font-size:13px;margin-bottom:24px}
  label{display:block;font-size:11px;text-transform:uppercase;letter-spacing:.15em;
        color:#7d8896;margin:18px 0 6px}
  input{background:#1a2230;border:none;color:#e6edf3;padding:14px;border-radius:12px;
        width:100%;font-size:16px;box-sizing:border-box}
  button{background:#3ea6ff;color:#0a0e14;font-weight:600;padding:14px;border-radius:999px;
         border:none;width:100%;font-size:16px;margin-top:24px}
  .ok{color:#3ea6ff;text-align:center;margin-top:18px}
  .err{color:#f87171;text-align:center;margin-top:18px}
</style></head>
<body>
<h1>Z1 Air</h1>
<div class="sub">Connect this unit to your home WiFi</div>
<form id="f">
  <label>Network</label>
  <select id="ssid" name="ssid" style="background:#1a2230;border:none;color:#e6edf3;padding:14px;border-radius:12px;width:100%;font-size:16px;box-sizing:border-box;appearance:none">
    <option value="">Scanning...</option>
  </select>
  <label>Password</label>
  <input name="pass" id="pass" type="password" required>
  <button>Connect</button>
</form>
<div id="msg"></div>
<script>
async function loadNets(){
  const sel = document.getElementById('ssid');
  for (let attempt = 0; attempt < 6; attempt++) {
    try {
      const r = await fetch('/scan');
      if (r.status === 200) {
        const list = await r.json();
        const seen = new Set();
        const opts = ['<option value="">Pick your network</option>'];
        list.sort((a,b) => b.rssi - a.rssi).forEach(n => {
          if (!n.ssid || seen.has(n.ssid)) return;
          seen.add(n.ssid);
          opts.push('<option value="' + n.ssid + '">' + n.ssid + '</option>');
        });
        opts.push('<option value="__manual">Other (type manually)</option>');
        sel.innerHTML = opts.join('');
        return;
      }
    } catch (e) {}
    await new Promise(r => setTimeout(r, 1500));
  }
}
loadNets();
document.getElementById('ssid').addEventListener('change', (e) => {
  if (e.target.value === '__manual') {
    const v = prompt('Enter network name');
    if (v) {
      const opt = document.createElement('option');
      opt.value = v; opt.textContent = v; opt.selected = true;
      e.target.appendChild(opt);
    } else { e.target.value = ''; }
  }
});
document.getElementById('f').addEventListener('submit', async (e) => {
  e.preventDefault();
  const ssid = document.getElementById('ssid').value;
  const pass = document.getElementById('pass').value;
  document.getElementById('msg').className = '';
  document.getElementById('msg').textContent = 'Saving...';
  try {
    const r = await fetch('/provision', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({ssid, pass})
    });
    if (r.ok) {
      const data = await r.json();
      document.getElementById('f').style.display = 'none';
      document.getElementById('msg').className = 'ok';
      document.getElementById('msg').innerHTML =
        '<div style="font-size:18px;font-weight:600;margin-bottom:12px">Connected!</div>' +
        '<div style="margin-bottom:16px">Your Z1 Air unit will join your WiFi in a few seconds.</div>' +
        '<div style="margin-bottom:8px;font-size:13px;color:#7d8896">Next steps:</div>' +
        '<div style="font-size:14px">1. Go back to your WiFi settings</div>' +
        '<div style="font-size:14px">2. Rejoin your home network</div>' +
        '<div style="font-size:14px">3. Open the Z1 Air app to control your AC</div>';
    } else {
      document.getElementById('msg').className = 'err';
      document.getElementById('msg').textContent = 'Failed to save.';
    }
  } catch (err) {
    document.getElementById('msg').className = 'err';
    document.getElementById('msg').textContent = 'Failed to save.';
  }
});
</script></body></html>
)HTML";

void SoftApProvisioner::begin(const String& ap_name, OnProvisioned cb) {
  g_cb = cb;
  active_ = true;

  WiFi.mode(WIFI_AP_STA);   // STA needed so we can scan nearby networks
  WiFi.softAP(ap_name.c_str());                         // open network (no password)
  WiFi.scanNetworks(true);                              // kick off async background scan
  IPAddress ip = WiFi.softAPIP();
  Serial.printf("[softap] %s ip=%s\n", ap_name.c_str(), ip.toString().c_str());

  // Catch-all DNS so any URL (e.g. captive.apple.com) routes to our IP — that
  // triggers iOS/Android's captive-portal popup.
  g_dns.start(53, "*", ip);

  g_ap_server.onNotFound([](AsyncWebServerRequest* r){ r->redirect("http://192.168.4.1/"); });
  g_ap_server.on("/", HTTP_GET, [](AsyncWebServerRequest* r){
    r->send(200, "text/html", PAGE_HTML);
  });
  // Captive-portal probe handling.  KEY: iOS treats anything OTHER than
  // exactly `<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>`
  // for /hotspot-detect.html as "captive present" and pops the popup.  Returning
  // our portal HTML (not a redirect) is the reliable trigger.
  auto serveHtml = [](AsyncWebServerRequest* r){ r->send(200, "text/html", PAGE_HTML); };
  g_ap_server.on("/hotspot-detect.html",       HTTP_GET, serveHtml);
  g_ap_server.on("/library/test/success.html", HTTP_GET, serveHtml);
  // Android: 204 = no portal; anything else triggers popup. Send 200 with body.
  g_ap_server.on("/generate_204",              HTTP_GET, serveHtml);
  g_ap_server.on("/gen_204",                   HTTP_GET, serveHtml);
  // Windows / others.
  g_ap_server.on("/connecttest.txt",           HTTP_GET, serveHtml);
  g_ap_server.on("/ncsi.txt",                  HTTP_GET, serveHtml);
  g_ap_server.on("/redirect",                  HTTP_GET, serveHtml);

  // WiFi scan results — returns JSON list of nearby networks.
  g_ap_server.on("/scan", HTTP_GET, [](AsyncWebServerRequest* r){
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) { r->send(202, "application/json", "[]"); return; }
    if (n < 0) { WiFi.scanNetworks(true); r->send(202, "application/json", "[]"); return; }
    String out = "[";
    for (int i = 0; i < n; ++i) {
      if (i) out += ",";
      out += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    }
    out += "]";
    WiFi.scanDelete();
    WiFi.scanNetworks(true);  // refresh for next request
    r->send(200, "application/json", out);
  });
  g_ap_server.on("/provision", HTTP_POST,
    [](AsyncWebServerRequest*){},
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t){
      String body((const char*)data, len);
      int sIdx = body.indexOf("\"ssid\":\"");
      int pIdx = body.indexOf("\"pass\":\"");
      if (sIdx < 0 || pIdx < 0) { req->send(400, "application/json", "{\"error\":\"bad json\"}"); return; }
      sIdx += 8;
      int sEnd = body.indexOf('"', sIdx);
      pIdx += 8;
      int pEnd = body.indexOf('"', pIdx);
      if (sEnd < 0 || pEnd < 0) { req->send(400, "application/json", "{\"error\":\"bad json\"}"); return; }
      String ssid = body.substring(sIdx, sEnd);
      String pass = body.substring(pIdx, pEnd);
      Serial.printf("[softap] received ssid=%s\n", ssid.c_str());
      // Reply with a payload the page can use to show "Open the app" / store links.
      req->send(200, "application/json",
        "{\"ok\":true,\"app\":{\"ios\":\"https://apps.apple.com/app/z1-air\","
        "\"android\":\"https://play.google.com/store/apps/details?id=com.z1air.app\","
        "\"web\":\"http://zoneair-unit.local/\"}}");
      if (g_cb) g_cb(ssid, pass);
    });
  g_ap_server.begin();
}

void SoftApProvisioner::poll() {
  if (active_) g_dns.processNextRequest();
}

}
