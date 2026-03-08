#include "config.h"
#include "webserver.h"
#include "steppers.h"
#include "commands.h"
#include "logbuf.h"
#include "webpage.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

static AsyncWebServer server(80);

// ─── Helpers ────────────────────────────────────────────────────────────────

static void jsonOk(AsyncWebServerRequest *req, const char *extra = nullptr)
{
    String s = "{\"ok\":true";
    if (extra)
    {
        s += ",";
        s += extra;
    }
    s += "}";
    req->send(200, "application/json", s);
}

static void jsonErr(AsyncWebServerRequest *req, int code, const char *msg)
{
    String s = "{\"ok\":false,\"error\":\"";
    s += msg;
    s += "\"}";
    req->send(code, "application/json", s);
}

// ─── Upload body handler ───────────────────────────────────────────────────
// ESPAsyncWebServer delivers large bodies in chunks via onBody callback.

static String uploadBuf;

static void onUploadBody(AsyncWebServerRequest *req, uint8_t *data, size_t len,
                         size_t index, size_t total)
{
    if (index == 0)
    {
        uploadBuf = "";
        uploadBuf.reserve(total < 131072 ? total : 131072);
    }
    uploadBuf += String((const char *)data).substring(0, len);
}

// ─── Route handlers ─────────────────────────────────────────────────────────

static void handleIndex(AsyncWebServerRequest *req)
{
    req->send(200, "text/html", INDEX_HTML);
}

static void handleStatus(AsyncWebServerRequest *req)
{
    JsonDocument doc;
    doc["state"] = (uint8_t)commandsState();
    doc["slot"] = turntableCurrentSlot();
    doc["pegs"] = numPegs;
    doc["homed"] = threaderIsHomed();
    doc["cmdIndex"] = commandsIndex();
    doc["cmdCount"] = commandsCount();
    doc["ttDeg"] = turntableTrueDeg();
    doc["thDeg"] = threaderTrueDeg();
    doc["cmdDelay"] = cmdDelaySec;
    doc["speedPct"] = speedPct;
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
}

static void handleLog(AsyncWebServerRequest *req)
{
    uint32_t since = 0;
    if (req->hasParam("since"))
        since = req->getParam("since")->value().toInt();

    LogEntry entries[50];
    uint32_t latest = 0;
    uint16_t n = logGetSince(since, entries, 50, &latest);

    JsonDocument doc;
    doc["latest"] = latest;
    JsonArray arr = doc["entries"].to<JsonArray>();
    for (uint16_t i = 0; i < n; i++)
    {
        JsonObject o = arr.add<JsonObject>();
        o["id"] = entries[i].id;
        o["msg"] = entries[i].msg;
    }
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
}

static void handleConfigGet(AsyncWebServerRequest *req)
{
    JsonDocument doc;
    doc["pegs"] = numPegs;
    doc["ttGearRatio"] = ttGearRatio;
    doc["ttBacklash"] = ttBacklashDeg;
    doc["ttMs"] = ttMicrosteps;
    doc["thMs"] = thMicrosteps;
    doc["ttSpeed"] = ttSpeed;
    doc["ttAccel"] = ttAccel;
    doc["thSpeed"] = thSpeed;
    doc["thAccel"] = thAccel;
    doc["thHomeSpeed"] = thHomeSpeed;
    doc["thUpDeg"] = thUpDeg;
    doc["thCenterDeg"] = thCenterDeg;
    doc["thDownDeg"] = thDownDeg;
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
}

static void handleConfig(AsyncWebServerRequest *req, uint8_t *data, size_t len,
                         size_t index, size_t total)
{
    if (index + len < total)
        return; // wait for full body — small JSON

    // Reconstruct full body (for small payloads this is the only chunk)
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, (const char *)data, len);
    if (err)
    {
        jsonErr(req, 400, "Bad JSON");
        return;
    }

    if (doc["pegs"].is<uint16_t>())
    {
        uint16_t v = doc["pegs"];
        if (v >= 2 && v <= MAX_PEGS)
            numPegs = v;
    }
    if (doc["ttGearRatio"].is<float>())
    {
        double v = doc["ttGearRatio"];
        if (v >= 0.1 && v <= 100.0)
            ttGearRatio = v;
    }
    if (doc["ttBacklash"].is<float>())
    {
        float v = doc["ttBacklash"];
        if (v >= 0.0f && v <= 5.0f)
            ttBacklashDeg = v;
    }
    if (doc["ttMs"].is<uint16_t>())
        ttMicrosteps = doc["ttMs"];
    if (doc["thMs"].is<uint16_t>())
        thMicrosteps = doc["thMs"];
    if (doc["ttSpeed"].is<uint32_t>())
        ttSpeed = doc["ttSpeed"];
    if (doc["ttAccel"].is<uint32_t>())
        ttAccel = doc["ttAccel"];
    if (doc["thSpeed"].is<uint32_t>())
        thSpeed = doc["thSpeed"];
    if (doc["thAccel"].is<uint32_t>())
        thAccel = doc["thAccel"];
    if (doc["thHomeSpeed"].is<uint32_t>())
        thHomeSpeed = doc["thHomeSpeed"];

    steppersRecalc();
    jsonOk(req);
}

static void handleJog(AsyncWebServerRequest *req, uint8_t *data, size_t len,
                      size_t index, size_t total)
{
    if (index + len < total)
        return;
    JsonDocument doc;
    if (deserializeJson(doc, (const char *)data, len))
    {
        jsonErr(req, 400, "Bad JSON");
        return;
    }
    float deg = doc["degrees"] | 0.0f;
    turntableJog(deg);
    jsonOk(req);
}

static void handleGoto(AsyncWebServerRequest *req, uint8_t *data, size_t len,
                       size_t index, size_t total)
{
    if (index + len < total)
        return;
    JsonDocument doc;
    if (deserializeJson(doc, (const char *)data, len))
    {
        jsonErr(req, 400, "Bad JSON");
        return;
    }
    uint16_t slot = doc["slot"] | 0;
    turntableGoToSlot(slot);
    jsonOk(req);
}

static void handleZero(AsyncWebServerRequest *req)
{
    turntableZero();
    jsonOk(req);
}

static void handleHome(AsyncWebServerRequest *req)
{
    threaderHomeStart();
    jsonOk(req);
}

static void handleThreader(AsyncWebServerRequest *req, uint8_t *data, size_t len,
                           size_t index, size_t total)
{
    if (index + len < total)
        return;
    JsonDocument doc;
    if (deserializeJson(doc, (const char *)data, len))
    {
        jsonErr(req, 400, "Bad JSON");
        return;
    }
    const char *action = doc["action"] | "";
    if (strcmp(action, "down") == 0)
        threaderGoToDown();
    else if (strcmp(action, "up") == 0)
        threaderGoToUp();
    else if (strcmp(action, "center") == 0)
        threaderGoToCenter();
    else if (strcmp(action, "setDown") == 0)
    {
        thDownDeg = doc["value"] | thDownDeg;
        logMsg("Threader DOWN pos set to %.1f deg", thDownDeg);
    }
    else if (strcmp(action, "setUp") == 0)
    {
        thUpDeg = doc["value"] | thUpDeg;
        logMsg("Threader UP pos set to %.1f deg", thUpDeg);
    }
    else if (strcmp(action, "setCenter") == 0)
    {
        thCenterDeg = doc["value"] | thCenterDeg;
        logMsg("Threader CENTER pos set to %.1f deg", thCenterDeg);
    }
    jsonOk(req);
}

static void handleUpload(AsyncWebServerRequest *req)
{
    if (uploadBuf.length() == 0)
    {
        jsonErr(req, 400, "Empty body");
        return;
    }
    char errMsg[128] = {0};
    bool ok = commandsParse(uploadBuf.c_str(), uploadBuf.length(), errMsg, sizeof(errMsg));
    uploadBuf = ""; // free memory
    if (ok)
    {
        String extra = "\"count\":";
        extra += commandsCount();
        jsonOk(req, extra.c_str());
    }
    else
    {
        jsonErr(req, 400, errMsg);
    }
}

static void handleStart(AsyncWebServerRequest *req)
{
    commandsStart();
    jsonOk(req);
}
static void handlePause(AsyncWebServerRequest *req)
{
    commandsPause();
    jsonOk(req);
}
static void handleResume(AsyncWebServerRequest *req)
{
    commandsResume();
    jsonOk(req);
}
static void handleStop(AsyncWebServerRequest *req)
{
    commandsStop();
    jsonOk(req);
}

static void handleDebug(AsyncWebServerRequest *req, uint8_t *data, size_t len,
                        size_t index, size_t total)
{
    if (index + len < total)
        return;
    JsonDocument doc;
    if (deserializeJson(doc, (const char *)data, len))
    {
        jsonErr(req, 400, "Bad JSON");
        return;
    }
    if (doc["delay"].is<float>())
    {
        float v = doc["delay"];
        if (v >= 0.0f && v <= 60.0f)
            cmdDelaySec = v;
    }
    if (doc["speed"].is<int>())
    {
        int v = doc["speed"];
        if (v >= 1 && v <= 100)
        {
            speedPct = (uint8_t)v;
            steppersRecalc();
        }
    }
    logMsg("Debug: delay=%.1fs speed=%u%%", cmdDelaySec, speedPct);
    jsonOk(req);
}

// ─── WiFi + Server Init ────────────────────────────────────────────────────

void webserverInit()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    logMsg("Connecting to WiFi '%s'...", WIFI_SSID);

    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000)
    {
        delay(250);
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        logMsg("WiFi connected  IP: %s", WiFi.localIP().toString().c_str());
    }
    else
    {
        logMsg("WiFi FAILED — starting AP 'StringArt'");
        WiFi.mode(WIFI_AP);
        WiFi.softAP("StringArt", "stringart123");
        logMsg("AP IP: %s", WiFi.softAPIP().toString().c_str());
    }

    // ── Routes ──
    server.on("/", HTTP_GET, handleIndex);
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/log", HTTP_GET, handleLog);
    server.on("/api/config", HTTP_GET, handleConfigGet);

    server.on("/api/zero", HTTP_POST, handleZero);
    server.on("/api/home", HTTP_POST, handleHome);
    server.on("/api/start", HTTP_POST, handleStart);
    server.on("/api/pause", HTTP_POST, handlePause);
    server.on("/api/resume", HTTP_POST, handleResume);
    server.on("/api/stop", HTTP_POST, handleStop);

    server.on("/api/debug", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr, handleDebug);

    // Routes with JSON body
    server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *req) {}, // onRequest (noop — handled in onBody)
              nullptr,                                                     // onUpload
              handleConfig);                                               // onBody

    server.on("/api/jog", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr, handleJog);

    server.on("/api/goto", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr, handleGoto);

    server.on("/api/threader", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr, handleThreader);

    // Upload route — body comes in chunks
    server.on("/api/upload", HTTP_POST, handleUpload, nullptr, onUploadBody);

    server.begin();
    logMsg("Web server started on port 80");
}
