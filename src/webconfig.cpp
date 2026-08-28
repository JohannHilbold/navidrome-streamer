#include "webconfig.h"
#include "config.h"
#include "settings.h"
#include "api.h"
#include "player.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

static WebServer wsrv(80);

// ── HTML UI (PROGMEM) ──────────────────────────────────────────────────

static const char WEB_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Music Remote</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:#1a1a2e;color:#e0e0e0;max-width:520px;margin:0 auto;padding:12px}
h1{color:#fff;font-size:1.3em;text-align:center;padding:8px 0}
.tabs{display:flex;gap:4px;margin:8px 0}
.tab{flex:1;padding:8px;text-align:center;background:#16213e;border:none;color:#888;border-radius:8px 8px 0 0;cursor:pointer;font-size:.9em}
.tab.active{background:#0f3460;color:#fff}
.panel{display:none;background:#16213e;border-radius:0 0 8px 8px;padding:12px;margin-bottom:12px}
.panel.active{display:block}

.np{background:#0f3460;border-radius:10px;padding:12px;margin-bottom:12px;text-align:center}
.np img{width:160px;height:160px;border-radius:8px;object-fit:cover;background:#16213e;display:block;margin:0 auto 8px}
.np .title{font-size:1.1em;color:#fff;font-weight:600}
.np .artist{color:#aaa;font-size:.9em;margin:2px 0 8px}
.controls{display:flex;align-items:center;justify-content:center;gap:12px}
.controls button{background:none;border:none;color:#fff;font-size:1.6em;cursor:pointer;padding:6px 10px;border-radius:8px}
.controls button:active{background:#16213e}
.vol{display:flex;align-items:center;gap:8px;justify-content:center;margin-top:8px}
.vol input{width:140px;accent-color:#0f3460}
.vol span{color:#aaa;font-size:.85em;min-width:36px;text-align:center}

.item{padding:10px;margin:4px 0;background:#1a1a2e;border-radius:8px;cursor:pointer;display:flex;justify-content:space-between;align-items:center}
.item:hover{background:#0f3460}
.item .name{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.back{color:#888;font-size:.85em;cursor:pointer;margin-bottom:6px;display:inline-block}
.back:hover{color:#fff}
.empty{color:#666;text-align:center;padding:16px}

.radio-item{display:flex;justify-content:space-between;align-items:center;padding:8px 10px;margin:4px 0;background:#1a1a2e;border-radius:8px}
.radio-item .name{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;cursor:pointer}
.radio-item .name:hover{color:#fff}
.radio-btns{display:flex;gap:6px}
.btn{padding:6px 14px;border:none;border-radius:6px;color:#fff;cursor:pointer;font-size:.85em}
.btn-play{background:#1b5e20}
.btn-del{background:#8b0000}
.add-form{display:flex;flex-direction:column;gap:6px;margin-top:10px}
.add-form input{padding:8px;border:1px solid #333;border-radius:6px;background:#1a1a2e;color:#fff;font-size:.9em}
.btn-add{background:#0f3460;padding:8px;border:none;border-radius:6px;color:#fff;cursor:pointer;font-size:.9em}
</style></head><body>
<h1>&#127925; ESP32 Music</h1>

<div class="np" id="np">
 <img id="np-art" src="" alt="">
 <div class="title" id="np-title">--</div>
 <div class="artist" id="np-artist">--</div>
 <div class="controls">
  <button onclick="ctrl('prev')">&#9198;</button>
  <button id="pp-btn" onclick="ctrl('pause')">&#9654;</button>
  <button onclick="ctrl('next')">&#9197;</button>
  <button onclick="ctrl('stop')">&#9209;</button>
 </div>
 <div class="vol">
  <span>&#128264;</span>
  <input type="range" id="vol" min="0" max="21" value="10" oninput="setVol(this.value)">
  <span id="vol-val">10</span>
 </div>
</div>

<div class="tabs">
 <button class="tab active" onclick="showTab('fav')">Favorites</button>
 <button class="tab" onclick="showTab('artists')">Artists</button>
 <button class="tab" onclick="showTab('playlists')">Playlists</button>
 <button class="tab" onclick="showTab('radio')">Radio</button>
</div>

<div class="panel active" id="p-fav"><div class="empty">Loading...</div></div>
<div class="panel" id="p-artists"><div class="empty">Loading...</div></div>
<div class="panel" id="p-playlists"><div class="empty">Loading...</div></div>
<div class="panel" id="p-radio"><div class="empty">Loading...</div></div>

<script>
const $ = s => document.querySelector(s);
const $$ = s => document.querySelectorAll(s);

function showTab(t) {
  $$('.tab').forEach((b,i) => b.classList.toggle('active', ['fav','artists','playlists','radio'][i]===t));
  $$('.panel').forEach(p => p.classList.remove('active'));
  $('#p-'+t).classList.add('active');
  if (t==='fav' && !$('#p-fav').dataset.loaded) loadFav();
  if (t==='artists' && !$('#p-artists').dataset.loaded) loadArtists();
  if (t==='playlists' && !$('#p-playlists').dataset.loaded) loadPlaylists();
  if (t==='radio') loadRadio();
}

function api(path, opts) {
  return fetch('/api/'+path, opts).then(r => r.json()).catch(()=>({}));
}
function post(path, body) {
  return api(path, {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(body)});
}
function ctrl(c) { post('control/'+c); }
function setVol(v) {
  $('#vol-val').textContent = v;
  post('control/volume', {vol:parseInt(v)});
}

function renderItems(panel, items, onClick) {
  if (!items || !items.length) { panel.innerHTML='<div class="empty">Nothing here</div>'; return; }
  panel.innerHTML = items.map((it,i) =>
    '<div class="item" data-i="'+i+'"><span class="name">'+esc(it.name)+'</span></div>'
  ).join('');
  panel.querySelectorAll('.item').forEach((el,i) =>
    el.addEventListener('click', () => onClick(items[i], i))
  );
}

function esc(s) { const d=document.createElement('div'); d.textContent=s; return d.innerHTML; }

async function loadFav() {
  const d = await api('browse/starred');
  $('#p-fav').dataset.loaded = '1';
  renderItems($('#p-fav'), d.items, (it) => loadAlbum(it.id, '#p-fav'));
}

async function loadArtists() {
  const d = await api('browse/artists');
  $('#p-artists').dataset.loaded = '1';
  renderItems($('#p-artists'), d.items, (it) => loadArtistAlbums(it.id, it.name));
}

async function loadArtistAlbums(id, name) {
  const p = $('#p-artists');
  p.innerHTML = '<span class="back" onclick="loadArtists()">&#8592; Artists</span>';
  const d = await api('browse/artist?id='+id);
  const c = document.createElement('div');
  renderItems(c, d.items, (it) => loadAlbum(it.id, '#p-artists', () => loadArtistAlbums(id, name)));
  p.appendChild(c);
}

async function loadAlbum(id, panelSel, backFn) {
  const p = $(panelSel);
  const back = backFn || (() => { delete p.dataset.loaded; showTab(panelSel==='#p-fav'?'fav':'artists'); });
  p.innerHTML = '<span class="back">&#8592; Back</span>';
  p.querySelector('.back').onclick = back;
  const d = await api('browse/album?id='+id);
  const c = document.createElement('div');
  renderItems(c, d.items, (it, i) => post('play/album', {id, startIndex:i}));
  p.appendChild(c);
}

async function loadPlaylists() {
  const d = await api('browse/playlists');
  $('#p-playlists').dataset.loaded = '1';
  renderItems($('#p-playlists'), d.items, (it) => loadPlaylist(it.id));
}

async function loadPlaylist(id) {
  const p = $('#p-playlists');
  p.innerHTML = '<span class="back" onclick="loadPlaylists()">&#8592; Playlists</span>';
  const d = await api('browse/playlist?id='+id);
  const c = document.createElement('div');
  renderItems(c, d.items, (it, i) => post('play/playlist', {id, startIndex:i}));
  p.appendChild(c);
}

async function loadRadio() {
  const d = await api('radio');
  const p = $('#p-radio');
  let h = '';
  if (d.stations && d.stations.length) {
    d.stations.forEach((s,i) => {
      h += '<div class="radio-item"><span class="name" onclick="playRadio('+i+')">'+esc(s.name)+'</span>';
      h += '<div class="radio-btns"><button class="btn btn-play" onclick="playRadio('+i+')">&#9654;</button>';
      h += '<button class="btn btn-del" onclick="delRadio('+i+')">&#10005;</button></div></div>';
    });
  } else {
    h = '<div class="empty">No stations saved</div>';
  }
  h += '<div class="add-form"><input id="rn" placeholder="Station name"><input id="ru" placeholder="Stream URL">';
  h += '<button class="btn-add" onclick="addRadio()">Add Station</button></div>';
  p.innerHTML = h;
}

async function playRadio(i) {
  const d = await api('radio');
  if (d.stations && d.stations[i]) post('play/radio', d.stations[i]);
}
async function addRadio() {
  const name=$('#rn').value.trim(), url=$('#ru').value.trim();
  if (name && url) { await post('radio/add', {name, url}); loadRadio(); }
}
async function delRadio(i) { await post('radio/remove', {index:i}); loadRadio(); }

let lastCover = '';
async function pollStatus() {
  try {
    const s = await api('status');
    $('#np-title').textContent = s.title || '--';
    $('#np-artist').textContent = s.artist || '--';
    $('#pp-btn').textContent = s.playing ? '⏸' : '▶';
    $('#vol').value = s.volume || 0;
    $('#vol-val').textContent = s.volume || 0;
    const cover = s.coverArt || '';
    if (cover && cover !== lastCover) {
      $('#np-art').src = '/api/coverart?id='+cover;
      lastCover = cover;
    } else if (!cover && lastCover) {
      $('#np-art').src = '';
      lastCover = '';
    }
  } catch(e) {}
}
setInterval(pollStatus, 2000);
pollStatus();
loadFav();
</script></body></html>
)rawhtml";

// ── Helpers ────────────────────────────────────────────────────────────

static void sendJson(int code, const String& json) {
    wsrv.send(code, "application/json", json);
}

static void menuItemsToJson(MenuItem* items, int count, String& out) {
    out = "{\"items\":[";
    for (int i = 0; i < count; i++) {
        if (i > 0) out += ',';
        out += "{\"id\":\"";
        out += items[i].id;
        out += "\",\"name\":\"";
        String name = items[i].name;
        name.replace("\"", "\\\"");
        out += name;
        out += "\"}";
    }
    out += "]}";
}

static MenuItem* webMenuBuf = NULL;

static void ensureMenuBuf() {
    if (!webMenuBuf)
        webMenuBuf = (MenuItem*)ps_malloc(500 * sizeof(MenuItem));
}

// ── Route handlers ─────────────────────────────────────────────────────

static void handlePage() {
    wsrv.send_P(200, "text/html", WEB_PAGE);
}

static void handleStatus() {
    String j = "{\"title\":\"";
    String t = playerCurrentTitle();
    t.replace("\"", "\\\"");
    j += t;
    j += "\",\"artist\":\"";
    String a = playerCurrentArtist();
    a.replace("\"", "\\\"");
    j += a;
    j += "\",\"playing\":";
    j += playerIsPlaying() ? "true" : "false";
    j += ",\"volume\":";
    j += String(playerGetVolume());
    j += ",\"radio\":";
    j += playerIsRadio() ? "true" : "false";
    j += ",\"coverArt\":\"";
    j += playerCurrentCoverArt();
    j += "\",\"index\":";
    j += String(playerCurrentIndex());
    j += ",\"queueSize\":";
    j += String(playerQueueSize());
    j += "}";
    sendJson(200, j);
}

static void handleBrowseStarred() {
    ensureMenuBuf();
    int n = apiGetStarredAlbums(webMenuBuf, 500);
    String j; menuItemsToJson(webMenuBuf, n, j);
    sendJson(200, j);
}

static void handleBrowseArtists() {
    ensureMenuBuf();
    int n = apiGetArtists(webMenuBuf, 500);
    String j; menuItemsToJson(webMenuBuf, n, j);
    sendJson(200, j);
}

static void handleBrowseArtist() {
    String id = wsrv.arg("id");
    if (id.isEmpty()) { sendJson(400, "{\"error\":\"missing id\"}"); return; }
    ensureMenuBuf();
    int n = apiGetArtistAlbums(id.c_str(), webMenuBuf, 500);
    String j; menuItemsToJson(webMenuBuf, n, j);
    sendJson(200, j);
}

static void handleBrowseAlbum() {
    String id = wsrv.arg("id");
    if (id.isEmpty()) { sendJson(400, "{\"error\":\"missing id\"}"); return; }
    ensureMenuBuf();
    int n = apiGetAlbumSongs(id.c_str(), webMenuBuf, 500);
    String j; menuItemsToJson(webMenuBuf, n, j);
    sendJson(200, j);
}

static void handleBrowsePlaylists() {
    ensureMenuBuf();
    int n = apiGetPlaylists(webMenuBuf, 500);
    String j; menuItemsToJson(webMenuBuf, n, j);
    sendJson(200, j);
}

static void handleBrowsePlaylist() {
    String id = wsrv.arg("id");
    if (id.isEmpty()) { sendJson(400, "{\"error\":\"missing id\"}"); return; }
    ensureMenuBuf();
    int n = apiGetPlaylistSongs(id.c_str(), webMenuBuf, 500);
    String j; menuItemsToJson(webMenuBuf, n, j);
    sendJson(200, j);
}

static void handlePlayAlbum() {
    JsonDocument doc;
    if (deserializeJson(doc, wsrv.arg("plain"))) { sendJson(400, "{\"error\":\"bad json\"}"); return; }
    const char* id = doc["id"];
    int startIndex = doc["startIndex"] | 0;
    if (!id) { sendJson(400, "{\"error\":\"missing id\"}"); return; }
    bool ok = playerLoadAlbum(id, startIndex);
    sendJson(200, ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

static void handlePlayPlaylist() {
    JsonDocument doc;
    if (deserializeJson(doc, wsrv.arg("plain"))) { sendJson(400, "{\"error\":\"bad json\"}"); return; }
    const char* id = doc["id"];
    int startIndex = doc["startIndex"] | 0;
    if (!id) { sendJson(400, "{\"error\":\"missing id\"}"); return; }
    bool ok = playerLoadPlaylist(id, startIndex);
    sendJson(200, ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

static void handlePlayRadio() {
    JsonDocument doc;
    if (deserializeJson(doc, wsrv.arg("plain"))) { sendJson(400, "{\"error\":\"bad json\"}"); return; }
    const char* name = doc["name"];
    const char* url = doc["url"];
    if (!name || !url) { sendJson(400, "{\"error\":\"missing name/url\"}"); return; }
    playerPlayRadio(name, url);
    sendJson(200, "{\"ok\":true}");
}

static void handleControlPause() { playerTogglePause(); sendJson(200, "{\"ok\":true}"); }
static void handleControlNext()  { playerPlayNext();     sendJson(200, "{\"ok\":true}"); }
static void handleControlPrev()  { playerPlayPrev();     sendJson(200, "{\"ok\":true}"); }
static void handleControlStop()  { playerStop();         sendJson(200, "{\"ok\":true}"); }

static void handleControlVolume() {
    JsonDocument doc;
    if (deserializeJson(doc, wsrv.arg("plain"))) { sendJson(400, "{\"error\":\"bad json\"}"); return; }
    int vol = doc["vol"] | -1;
    if (vol < 0) { sendJson(400, "{\"error\":\"missing vol\"}"); return; }
    playerSetVolume(vol);
    sendJson(200, "{\"ok\":true}");
}

static void handleRadioList() {
    int count = settingsGetRadioCount();
    String j = "{\"stations\":[";
    for (int i = 0; i < count; i++) {
        RadioStation r = settingsGetRadio(i);
        if (i > 0) j += ',';
        j += "{\"name\":\"";
        String name = r.name;
        name.replace("\"", "\\\"");
        j += name;
        j += "\",\"url\":\"";
        String url = r.url;
        url.replace("\"", "\\\"");
        j += url;
        j += "\"}";
    }
    j += "]}";
    sendJson(200, j);
}

static void handleRadioAdd() {
    JsonDocument doc;
    if (deserializeJson(doc, wsrv.arg("plain"))) { sendJson(400, "{\"error\":\"bad json\"}"); return; }
    const char* name = doc["name"];
    const char* url = doc["url"];
    if (!name || !url) { sendJson(400, "{\"error\":\"missing name/url\"}"); return; }
    settingsAddRadio(name, url);
    sendJson(200, "{\"ok\":true}");
}

static void handleRadioRemove() {
    JsonDocument doc;
    if (deserializeJson(doc, wsrv.arg("plain"))) { sendJson(400, "{\"error\":\"bad json\"}"); return; }
    int index = doc["index"] | -1;
    if (index < 0) { sendJson(400, "{\"error\":\"missing index\"}"); return; }
    settingsRemoveRadio(index);
    sendJson(200, "{\"ok\":true}");
}

static void handleCoverArt() {
    String id = wsrv.arg("id");
    if (id.isEmpty()) { wsrv.send(400, "text/plain", "missing id"); return; }

    String url = buildApiUrl("getCoverArt.view") + "&id=" + id + "&size=200";
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    if (!http.begin(client, url)) { wsrv.send(502, "text/plain", "fetch failed"); return; }
    int code = http.GET();
    if (code != 200) { http.end(); wsrv.send(502, "text/plain", "upstream error"); return; }

    int len = http.getSize();
    String ct = http.header("Content-Type");
    if (ct.isEmpty()) ct = "image/jpeg";

    WiFiClient* stream = http.getStreamPtr();
    wsrv.setContentLength(len > 0 ? len : CONTENT_LENGTH_UNKNOWN);
    wsrv.send(200, ct, "");

    uint8_t buf[512];
    while (stream->connected() || stream->available()) {
        int avail = stream->available();
        if (avail <= 0) { delay(1); continue; }
        int n = stream->readBytes(buf, min(avail, (int)sizeof(buf)));
        if (n <= 0) break;
        wsrv.client().write(buf, n);
    }
    http.end();
}

// ── Start / Loop ───────────────────────────────────────────────────────

void webconfigStart() {
    wsrv.on("/", HTTP_GET, handlePage);

    wsrv.on("/api/status", HTTP_GET, handleStatus);
    wsrv.on("/api/browse/starred", HTTP_GET, handleBrowseStarred);
    wsrv.on("/api/browse/artists", HTTP_GET, handleBrowseArtists);
    wsrv.on("/api/browse/artist", HTTP_GET, handleBrowseArtist);
    wsrv.on("/api/browse/album", HTTP_GET, handleBrowseAlbum);
    wsrv.on("/api/browse/playlists", HTTP_GET, handleBrowsePlaylists);
    wsrv.on("/api/browse/playlist", HTTP_GET, handleBrowsePlaylist);

    wsrv.on("/api/play/album", HTTP_POST, handlePlayAlbum);
    wsrv.on("/api/play/playlist", HTTP_POST, handlePlayPlaylist);
    wsrv.on("/api/play/radio", HTTP_POST, handlePlayRadio);

    wsrv.on("/api/control/pause", HTTP_POST, handleControlPause);
    wsrv.on("/api/control/next", HTTP_POST, handleControlNext);
    wsrv.on("/api/control/prev", HTTP_POST, handleControlPrev);
    wsrv.on("/api/control/stop", HTTP_POST, handleControlStop);
    wsrv.on("/api/control/volume", HTTP_POST, handleControlVolume);

    wsrv.on("/api/radio", HTTP_GET, handleRadioList);
    wsrv.on("/api/radio/add", HTTP_POST, handleRadioAdd);
    wsrv.on("/api/radio/remove", HTTP_POST, handleRadioRemove);

    wsrv.on("/api/coverart", HTTP_GET, handleCoverArt);

    wsrv.begin();
    Serial.printf("[webconfig] Started on http://%s/\n", WiFi.localIP().toString().c_str());
}

void webconfigLoop() {
    wsrv.handleClient();
}
