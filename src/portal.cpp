#include "portal.h"
#include "settings.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

static WebServer server(80);
static DNSServer dns;
static bool active = false;

static const char PAGE_HEADER[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Music Setup</title><style>
*{box-sizing:border-box;font-family:system-ui,sans-serif}
body{margin:0;padding:16px;background:#1a1a2e;color:#e0e0e0;max-width:480px;margin:0 auto}
h2{color:#fff;margin-top:24px;border-bottom:1px solid #333;padding-bottom:8px}
h3{color:#ccc;margin:16px 0 8px}
.net{padding:10px;margin:4px 0;background:#16213e;border-radius:8px;cursor:pointer;display:flex;justify-content:space-between}
.net:hover{background:#0f3460}
.saved{display:flex;justify-content:space-between;align-items:center;padding:8px 10px;margin:4px 0;background:#1a3a1a;border-radius:8px}
input[type=text],input[type=password],input[type=url]{width:100%;padding:10px;margin:6px 0;border:1px solid #333;border-radius:6px;background:#16213e;color:#fff;font-size:16px}
button,.btn{display:inline-block;padding:10px 20px;margin:8px 4px 8px 0;border:none;border-radius:6px;font-size:16px;cursor:pointer;color:#fff;text-decoration:none}
.btn-add{background:#0f3460}.btn-del{background:#8b0000;padding:6px 12px;font-size:14px}
.btn-save{background:#1b5e20;width:100%}.btn-reboot{background:#b71c1c;width:100%}
.signal{color:#888;font-size:14px}
</style>
<script>
function selectNet(ssid){document.getElementById('ssid').value=ssid;document.getElementById('pass').focus()}
</script>
</head><body><h1>ESP32 Music</h1>
)rawhtml";

static const char PAGE_FOOTER[] PROGMEM = R"rawhtml(
</body></html>
)rawhtml";

static void buildPage(String& page) {
    page = FPSTR(PAGE_HEADER);

    // Saved networks
    page += "<h2>Saved Networks</h2>";
    int count = settingsGetWifiCount();
    if (count == 0) {
        page += "<p style='color:#888'>No networks saved</p>";
    }
    for (int i = 0; i < count; i++) {
        WifiNetwork n = settingsGetWifi(i);
        page += "<div class='saved'><span>";
        page += n.ssid;
        page += "</span><form method='POST' action='/removewifi' style='margin:0'>";
        page += "<input type='hidden' name='idx' value='";
        page += String(i);
        page += "'><button type='submit' class='btn btn-del'>Remove</button></form></div>";
    }

    // WiFi scan
    page += "<h2>Available Networks</h2>";
    int n = WiFi.scanNetworks();
    if (n > 0) {
        for (int i = 0; i < n; i++) {
            page += "<div class='net' onclick=\"selectNet('";
            String ssid = WiFi.SSID(i);
            ssid.replace("'", "\\'");
            page += ssid;
            page += "')\">";
            page += "<span>" + WiFi.SSID(i) + "</span>";
            page += "<span class='signal'>" + String(WiFi.RSSI(i)) + " dBm</span>";
            page += "</div>";
        }
    } else {
        page += "<p style='color:#888'>No networks found</p>";
    }
    WiFi.scanDelete();

    // Add network form
    page += "<h3>Add Network</h3>";
    page += "<form method='POST' action='/addwifi'>";
    page += "<input type='text' id='ssid' name='ssid' placeholder='SSID' required>";
    page += "<input type='password' id='pass' name='pass' placeholder='Password'>";
    page += "<button type='submit' class='btn btn-add'>Add Network</button></form>";

    // Navidrome config
    NavidromeConfig nd = settingsGetNavidrome();
    page += "<h2>Navidrome Server</h2>";
    page += "<form method='POST' action='/savenavidrome'>";
    page += "<input type='url' name='url' placeholder='https://your-server.com' value='";
    page += nd.url;
    page += "' required>";
    page += "<input type='text' name='user' placeholder='Username' value='";
    page += nd.username;
    page += "' required>";
    page += "<input type='password' name='pass' placeholder='Password' value='";
    page += nd.password;
    page += "' required>";
    page += "<button type='submit' class='btn btn-save'>Save Server</button></form>";

    // Reboot
    page += "<h2>Apply</h2>";
    page += "<form method='POST' action='/reboot'>";
    page += "<button type='submit' class='btn btn-reboot'>Save &amp; Reboot</button></form>";

    page += FPSTR(PAGE_FOOTER);
}

static void handleRoot() {
    String page;
    buildPage(page);
    server.send(200, "text/html", page);
}

static void handleAddWifi() {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    if (ssid.length() > 0) {
        settingsAddWifi(ssid.c_str(), pass.c_str());
    }
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
}

static void handleRemoveWifi() {
    int idx = server.arg("idx").toInt();
    settingsRemoveWifi(idx);
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
}

static void handleSaveNavidrome() {
    String url = server.arg("url");
    String user = server.arg("user");
    String pass = server.arg("pass");
    settingsSetNavidrome(url.c_str(), user.c_str(), pass.c_str());
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
}

static void handleReboot() {
    server.send(200, "text/html", "<html><body style='background:#1a1a2e;color:#fff;font-family:sans-serif;text-align:center;padding-top:100px'><h1>Rebooting...</h1><p>Device will restart now.</p></body></html>");
    delay(1000);
    ESP.restart();
}

static void handleCaptive() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
}

void portalStart() {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("ESP32-Music");
    delay(100);
    Serial.printf("[portal] AP started, IP: %s\n", WiFi.softAPIP().toString().c_str());

    dns.start(53, "*", WiFi.softAPIP());

    server.on("/", HTTP_GET, handleRoot);
    server.on("/addwifi", HTTP_POST, handleAddWifi);
    server.on("/removewifi", HTTP_POST, handleRemoveWifi);
    server.on("/savenavidrome", HTTP_POST, handleSaveNavidrome);
    server.on("/reboot", HTTP_POST, handleReboot);
    server.onNotFound(handleCaptive);
    server.begin();

    active = true;
    Serial.println("[portal] Web server started");
}

void portalLoop() {
    dns.processNextRequest();
    server.handleClient();
}

bool portalIsActive() {
    return active;
}
