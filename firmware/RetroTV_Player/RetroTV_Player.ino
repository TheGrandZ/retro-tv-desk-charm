// RetroTV Player
// Plays the .mjpeg clips in the /mjpeg folder of the SD card, and hosts a small web page
// so a phone can convert a video and send it to the SD card over WiFi (no computer needed).
//
// Based on the ESP32-2432S028 video player by The Last Outpost Workshop (MIT License):
// https://github.com/thelastoutpostworkshop/esp32-2432S028_video_player
//
// Board: "ESP32 Dev Module"
// Libraries (Library Manager): "GFX Library for Arduino" and "JPEGDEC"  (same two as before)
// WiFi, WebServer, ESPmDNS and SD come with the ESP32 board package. Nothing new to install.

#include <Arduino_GFX_Library.h>
#include "MjpegClass.h"
#include "webpage.h"
#include "SD.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>

// ======================= SETTINGS YOU MIGHT CHANGE =======================
#define SCREEN_ROTATION 1 // 1 or 3 for a sideways (landscape) screen. Use the number that worked in your old sketch.
#define INVERT_DISPLAY false // set to true if the colors look like a photo negative
#define BL_PIN 21            // backlight pin (27 on a few boards)
#define DISPLAY_SPI_SPEED 80000000L // try 40000000L if the picture is glitchy

const char *AP_NAME = "RetroTV";        // WiFi network the TV creates
const char *AP_PASSWORD = "retrotv123"; // at least 8 characters
const char *HOST_NAME = "retrotv";      // on home WiFi the page is http://retrotv.local
#define INFO_SCREEN_MS 6000             // how long the "how to connect" screen shows at power-on
// ==========================================================================

#define SD_CS 5
#define SD_MISO 19
#define SD_MOSI 23
#define SD_SCK 18
#define SD_SPI_SPEED 80000000L

#define SKIP_BUTTON_PIN 0
#define BOOT_BUTTON_DEBOUCE_TIME 400

const char *MJPEG_FOLDER = "/mjpeg";
const char *UPLOAD_TMP = "/mjpeg/incoming.tmp"; // uploads land here first, then get renamed
const char *WIFI_FILE = "/wifi.txt";            // optional: line 1 = WiFi name, line 2 = password

#define MAX_FILES 40
String mjpegFileList[MAX_FILES];
uint32_t mjpegFileSizes[MAX_FILES] = {0};
int mjpegCount = 0;
static int currentMjpegIndex = 0;
String nowPlaying = "";

MjpegClass mjpeg;
int total_frames;        // frames read from the clip (shown + skipped)
int shown_frames;        // frames actually drawn
int skipped_frames;      // frames skipped to keep the clip at the right speed
uint32_t draw_ms;        // time spent pushing pixels to the screen
unsigned long start_ms;
long output_buf_size, estimateBufferSize;
uint8_t *mjpeg_buf;
uint16_t *output_buf;

Arduino_DataBus *bus = new Arduino_HWSPI(2 /* DC */, 15 /* CS */, 14 /* SCK */, 13 /* MOSI */, 12 /* MISO */);
Arduino_GFX *gfx = new Arduino_ILI9341(bus);
SPIClass sd_spi(VSPI);

// ---- web server state ----
WebServer server(80);
String tvAddress = "";   // what to type in the phone's browser
bool onHomeWifi = false; // true = joined your WiFi, false = TV made its own network

// Things the web page asks for are done between clips, never while a file is open.
enum PendingOp
{
    OP_NONE,
    OP_FINISH_UPLOAD,
    OP_DELETE,
    OP_PLAY
};
PendingOp pendingOp = OP_NONE;
String pendingName = "";
bool stopPlayback = false;

File uploadFile;
bool uploadActive = false; // false = this upload is being ignored because the TV was busy
bool uploadOk = false;
String uploadTarget = "";
size_t uploadExpected = 0;
int uploadLastPct = -1;
bool idleScreenShown = false;

volatile bool skipRequested = false;
uint32_t lastPress = 0;

void IRAM_ATTR onButtonPress()
{
    skipRequested = true;
}

// ------------------------------------------------------------------ helpers

bool isMjpegVideoFileName(const String &fileName)
{
    int slashIndex = fileName.lastIndexOf('/');
    String baseName = slashIndex >= 0 ? fileName.substring(slashIndex + 1) : fileName;
    return baseName.endsWith(".mjpeg") && !baseName.startsWith("._");
}

String clipPath(const String &fileName)
{
    return String(MJPEG_FOLDER) + "/" + fileName;
}

// "beach.f12.mjpeg" -> 12.  0 means "no speed tag, play as fast as possible" (old clips).
int fpsFromName(const String &fileName)
{
    if (!fileName.endsWith(".mjpeg"))
        return 0;
    String stem = fileName.substring(0, fileName.length() - 6);
    int p = stem.lastIndexOf(".f");
    if (p < 0)
        return 0;
    String digits = stem.substring(p + 2);
    if (digits.length() == 0 || digits.length() > 2)
        return 0;
    for (unsigned int i = 0; i < digits.length(); i++)
    {
        if (!isDigit(digits[i]))
            return 0;
    }
    int fps = digits.toInt();
    return (fps >= 1 && fps <= 30) ? fps : 0;
}

// "beach.f12.mjpeg" -> "beach"
String clipBaseName(const String &fileName)
{
    String stem = fileName;
    if (stem.endsWith(".mjpeg"))
        stem = stem.substring(0, stem.length() - 6);
    if (fpsFromName(fileName) > 0)
        stem = stem.substring(0, stem.lastIndexOf(".f"));
    return stem;
}

// Keep only letters, numbers, _ and - so a name can never point outside the mjpeg folder.
String cleanName(const String &raw)
{
    String out = "";
    for (unsigned int i = 0; i < raw.length() && out.length() < 20; i++)
    {
        char c = raw[i];
        if (isAlphaNumeric(c) || c == '_' || c == '-')
            out += c;
    }
    if (out.length() == 0)
        out = "clip";
    return out;
}

int indexOfClip(const String &fileName)
{
    for (int i = 0; i < mjpegCount; i++)
    {
        if (mjpegFileList[i] == fileName)
            return i;
    }
    return -1;
}

void loadMjpegFilesList()
{
    mjpegCount = 0;
    File mjpegDir = SD.open(MJPEG_FOLDER);
    if (!mjpegDir || !mjpegDir.isDirectory())
    {
        Serial.printf("No %s folder\n", MJPEG_FOLDER);
        return;
    }
    while (true)
    {
        File file = mjpegDir.openNextFile();
        if (!file)
            break;
        if (!file.isDirectory())
        {
            String name = file.name();
            int slash = name.lastIndexOf('/');
            if (slash >= 0)
                name = name.substring(slash + 1);
            if (isMjpegVideoFileName(name) && mjpegCount < MAX_FILES)
            {
                mjpegFileList[mjpegCount] = name;
                mjpegFileSizes[mjpegCount] = file.size();
                mjpegCount++;
            }
        }
        file.close();
    }
    mjpegDir.close();
    Serial.printf("%d mjpeg files found\n", mjpegCount);
}

// ------------------------------------------------------------------ screens

void textLine(int16_t y, uint8_t size, uint16_t color, const String &text)
{
    gfx->setTextSize(size);
    gfx->setTextColor(color);
    gfx->setCursor(10, y);
    gfx->print(text);
}

void showInfoScreen(const char *title)
{
    gfx->fillScreen(RGB565_BLACK);
    textLine(12, 3, RGB565_RED, title);
    if (onHomeWifi)
    {
        textLine(60, 2, RGB565_WHITE, "On your phone, open:");
        textLine(90, 2, RGB565_YELLOW, String("http://") + HOST_NAME + ".local");
        textLine(120, 2, RGB565_WHITE, "or");
        textLine(150, 2, RGB565_YELLOW, "http://" + tvAddress);
    }
    else
    {
        textLine(60, 2, RGB565_WHITE, "1. Join this WiFi:");
        textLine(86, 2, RGB565_YELLOW, String("   ") + AP_NAME);
        textLine(112, 2, RGB565_WHITE, String("   pass: ") + AP_PASSWORD);
        textLine(150, 2, RGB565_WHITE, "2. In the browser open:");
        textLine(176, 2, RGB565_YELLOW, "   http://" + tvAddress);
    }
}

void showReceiving(int pct)
{
    if (pct == uploadLastPct)
        return;
    if (uploadLastPct < 0)
    {
        gfx->fillRect(20, 80, gfx->width() - 40, 80, RGB565_BLACK);
        gfx->drawRect(20, 80, gfx->width() - 40, 80, RGB565_WHITE);
        textLine(92, 2, RGB565_WHITE, "   Receiving clip...");
        gfx->drawRect(40, 124, gfx->width() - 80, 18, RGB565_WHITE);
    }
    uploadLastPct = pct;
    int w = (gfx->width() - 84) * constrain(pct, 0, 100) / 100;
    gfx->fillRect(42, 126, w, 14, RGB565_RED);
}

// ------------------------------------------------------------------ web page handlers

void handleRoot()
{
    server.send_P(200, "text/html", INDEX_HTML);
}

String jsonEscape(const String &s)
{
    String out = "";
    for (unsigned int i = 0; i < s.length(); i++)
    {
        char c = s[i];
        if (c == '"' || c == '\\')
            out += '\\';
        if ((uint8_t)c >= 32)
            out += c;
    }
    return out;
}

void handleList()
{
    String json = "{\"clips\":[";
    for (int i = 0; i < mjpegCount; i++)
    {
        if (i)
            json += ",";
        json += "{\"n\":\"" + jsonEscape(mjpegFileList[i]) + "\",\"s\":" + String(mjpegFileSizes[i]) + "}";
    }
    json += "],\"now\":\"" + jsonEscape(nowPlaying) + "\"}";
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", json);
}

// Shared by /play and /delete: the clip must be one we listed ourselves.
void queueClipOp(PendingOp op)
{
    String name = server.arg("name");
    if (pendingOp != OP_NONE)
    {
        server.send(409, "text/plain", "The TV is busy. Try again in a second.");
        return;
    }
    if (indexOfClip(name) < 0)
    {
        server.send(404, "text/plain", "That clip is not on the TV any more.");
        return;
    }
    pendingName = name;
    pendingOp = op;
    stopPlayback = true;
    server.send(200, "text/plain", "OK");
}

void handlePlay()
{
    queueClipOp(OP_PLAY);
}

void handleDelete()
{
    queueClipOp(OP_DELETE);
}

// Called many times while the phone is sending a clip.
void handleUploadData()
{
    HTTPUpload &up = server.upload();
    if (up.status == UPLOAD_FILE_START)
    {
        uploadOk = false;
        uploadLastPct = -1;
        uploadActive = (pendingOp == OP_NONE);
        if (!uploadActive)
            return; // still finishing the previous request: ignore this one, the phone gets an error
        int fps = constrain((int)server.arg("fps").toInt(), 0, 30);
        uploadTarget = cleanName(server.arg("name")) + (fps > 0 ? ".f" + String(fps) : String("")) + ".mjpeg";
        uploadExpected = (size_t)server.arg("size").toInt();
        if (!SD.exists(MJPEG_FOLDER))
            SD.mkdir(MJPEG_FOLDER);
        SD.remove(UPLOAD_TMP);
        uploadFile = SD.open(UPLOAD_TMP, FILE_WRITE);
        uploadOk = (bool)uploadFile;
        Serial.printf("Upload start: %s (%u bytes expected)\n", uploadTarget.c_str(), (unsigned)uploadExpected);
        showReceiving(0);
    }
    else if (!uploadActive)
    {
        return;
    }
    else if (up.status == UPLOAD_FILE_WRITE)
    {
        if (uploadOk && uploadFile.write(up.buf, up.currentSize) != up.currentSize)
        {
            uploadOk = false; // SD card full or pulled out
        }
        if (uploadExpected > 0)
            showReceiving((int)(100.0 * up.totalSize / uploadExpected));
    }
    else if (up.status == UPLOAD_FILE_END)
    {
        if (uploadFile)
            uploadFile.close();
        if (uploadOk && up.totalSize > 0 && (uploadExpected == 0 || up.totalSize == uploadExpected))
        {
            pendingName = uploadTarget;
            pendingOp = OP_FINISH_UPLOAD;
            stopPlayback = true;
            Serial.printf("Upload done: %u bytes\n", (unsigned)up.totalSize);
        }
        else
        {
            uploadOk = false;
            SD.remove(UPLOAD_TMP);
            stopPlayback = true; // restart the clip so the "Receiving" box is cleared
            idleScreenShown = false;
            Serial.println("Upload failed");
        }
    }
    else if (up.status == UPLOAD_FILE_ABORTED)
    {
        if (uploadFile)
            uploadFile.close();
        SD.remove(UPLOAD_TMP);
        uploadOk = false;
        stopPlayback = true; // restart the clip so the "Receiving" box is cleared
        idleScreenShown = false;
        Serial.println("Upload aborted");
    }
}

// Called once when the upload request is complete.
void handleUploadDone()
{
    if (uploadOk)
        server.send(200, "text/plain", "OK");
    else if (!uploadActive)
        server.send(409, "text/plain", "The TV is busy. Try again in a second.");
    else
        server.send(500, "text/plain", "The TV could not save the clip. The SD card may be full or missing.");
}

// ------------------------------------------------------------------ brightness

Preferences prefs;    // remembers the brightness when the TV is unplugged
int lightLevel = 255; // brightness set from the phone page (10..255)
float blNow = 255;    // brightness the backlight is at right now
int blWritten = -1;
uint32_t lightTick = 0;

// Called very often; does its work about ten times a second. Fades instead of jumping.
void updateBacklight()
{
    uint32_t now = millis();
    if (now - lightTick < 100)
        return;
    lightTick = now;
    float step = constrain((float)lightLevel - blNow, -6.0f, 6.0f);
    blNow += step;
    int out = constrain((int)(blNow + 0.5f), 10, 255);
    if (out != blWritten)
    {
        analogWrite(BL_PIN, out);
        blWritten = out;
    }
}

void loadLightSettings()
{
    prefs.begin("retrotv", false);
    lightLevel = constrain(prefs.getInt("level", 255), 10, 255);
    blNow = lightLevel;
}

// /light            -> current brightness
// /light?level=180  -> change it (10..255)
void handleLight()
{
    if (server.hasArg("level"))
    {
        lightLevel = constrain((int)server.arg("level").toInt(), 10, 255);
        prefs.putInt("level", lightLevel);
    }
    String json = "{\"level\":" + String(lightLevel) + ",\"now\":" + String(blWritten) + "}";
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", json);
}

void handleNotFound()
{
    server.send(404, "text/plain", "Not found");
}

// Runs between clips, when no clip file is open.
void runPendingOp()
{
    PendingOp op = pendingOp;
    String name = pendingName;

    if (op == OP_FINISH_UPLOAD)
    {
        // Remove any older clip with the same name (even if it had a different speed tag).
        String base = clipBaseName(name);
        for (int i = 0; i < mjpegCount; i++)
        {
            if (clipBaseName(mjpegFileList[i]) == base)
                SD.remove(clipPath(mjpegFileList[i]));
        }
        if (!SD.rename(UPLOAD_TMP, clipPath(name).c_str()))
            Serial.println("Rename of uploaded clip failed");
        loadMjpegFilesList();
        int idx = indexOfClip(name);
        currentMjpegIndex = idx >= 0 ? idx : 0;
    }
    else if (op == OP_DELETE)
    {
        SD.remove(clipPath(name));
        loadMjpegFilesList();
        if (currentMjpegIndex >= mjpegCount)
            currentMjpegIndex = 0;
    }
    else if (op == OP_PLAY)
    {
        int idx = indexOfClip(name);
        if (idx >= 0)
            currentMjpegIndex = idx;
    }

    pendingOp = OP_NONE;
    pendingName = "";
    stopPlayback = false;
    idleScreenShown = false;
}

// ------------------------------------------------------------------ WiFi

bool readWifiFile(String &ssid, String &pass)
{
    File f = SD.open(WIFI_FILE);
    if (!f || f.isDirectory())
        return false;
    ssid = f.readStringUntil('\n');
    pass = f.readStringUntil('\n');
    f.close();
    ssid.trim();
    pass.trim();
    return ssid.length() > 0;
}

void startWifi()
{
    String ssid, pass;
    if (readWifiFile(ssid, pass))
    {
        gfx->fillScreen(RGB565_BLACK);
        textLine(12, 3, RGB565_RED, "Retro TV");
        textLine(60, 2, RGB565_WHITE, "Joining WiFi:");
        textLine(86, 2, RGB565_YELLOW, ssid);
        WiFi.mode(WIFI_STA);
        WiFi.setHostname(HOST_NAME);
        WiFi.begin(ssid.c_str(), pass.c_str());
        uint32_t t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000)
        {
            delay(250);
        }
        if (WiFi.status() == WL_CONNECTED)
        {
            onHomeWifi = true;
            tvAddress = WiFi.localIP().toString();
            WiFi.setSleep(false); // faster uploads
            if (MDNS.begin(HOST_NAME))
                MDNS.addService("http", "tcp", 80);
            Serial.printf("Joined %s, address %s\n", ssid.c_str(), tvAddress.c_str());
            return;
        }
        Serial.println("Could not join the WiFi in wifi.txt, making my own network instead");
        WiFi.disconnect(true);
        delay(200);
    }
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_NAME, AP_PASSWORD);
    delay(200);
    onHomeWifi = false;
    tvAddress = WiFi.softAPIP().toString();
    Serial.printf("Created WiFi network %s, address %s\n", AP_NAME, tvAddress.c_str());
}

// Wait while still answering the phone.
void serveFor(uint32_t ms)
{
    uint32_t t0 = millis();
    while (millis() - t0 < ms && pendingOp == OP_NONE)
    {
        server.handleClient();
        updateBacklight();
        delay(2);
    }
}

// ------------------------------------------------------------------ setup / loop

void setup()
{
    Serial.begin(115200);

    loadLightSettings();
    analogWrite(BL_PIN, lightLevel);           // backlight on (dimmable)
    blWritten = lightLevel;

    Serial.println("Display initialization");
    if (!gfx->begin(DISPLAY_SPI_SPEED))
    {
        Serial.println("Display initialization failed!");
        while (true)
        {
            delay(1000);
        }
    }
    gfx->setRotation(SCREEN_ROTATION);
    gfx->invertDisplay(INVERT_DISPLAY);
    gfx->fillScreen(RGB565_BLACK);
    gfx->setTextWrap(false);
    Serial.printf("Screen size Width=%d,Height=%d\n", gfx->width(), gfx->height());

    Serial.println("SD Card initialization");
    sd_spi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, sd_spi, SD_SPI_SPEED, "/sd"))
    {
        Serial.println("ERROR: SD card mount failed!");
        textLine(12, 3, RGB565_RED, "No SD card");
        textLine(60, 2, RGB565_WHITE, "Put the card in and");
        textLine(86, 2, RGB565_WHITE, "unplug / replug the TV.");
        while (true)
        {
            delay(1000);
        }
    }
    if (!SD.exists(MJPEG_FOLDER))
        SD.mkdir(MJPEG_FOLDER);
    SD.remove(UPLOAD_TMP); // left over if power was cut during an upload

    Serial.println("Buffer allocation");
    output_buf_size = gfx->width() * 4 * 2;
    output_buf = (uint16_t *)heap_caps_aligned_alloc(16, output_buf_size * sizeof(uint16_t), MALLOC_CAP_DMA);
    estimateBufferSize = gfx->width() * gfx->height() * 2 / 5;
    mjpeg_buf = (uint8_t *)heap_caps_malloc(estimateBufferSize, MALLOC_CAP_8BIT);
    if (!output_buf || !mjpeg_buf)
    {
        Serial.println("Buffer allocation failed!");
        while (true)
        {
            delay(1000);
        }
    }

    loadMjpegFilesList();

    startWifi();
    server.on("/", HTTP_GET, handleRoot);
    server.on("/list", HTTP_GET, handleList);
    server.on("/play", HTTP_GET, handlePlay);
    server.on("/delete", HTTP_GET, handleDelete);
    server.on("/light", HTTP_GET, handleLight);
    server.on("/upload", HTTP_POST, handleUploadDone, handleUploadData);
    server.onNotFound(handleNotFound);
    server.enableDelay(false); // do not slow the video down while nobody is connected
    server.begin();

    showInfoScreen("Retro TV");
    serveFor(INFO_SCREEN_MS);

    pinMode(SKIP_BUTTON_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(SKIP_BUTTON_PIN), onButtonPress, FALLING);
    Serial.printf("Free memory: %u bytes\n", (unsigned)ESP.getFreeHeap());
}

void loop()
{
    // Finish whatever the phone asked for BEFORE listening for the next request,
    // so two requests can never overlap.
    if (pendingOp != OP_NONE)
        runPendingOp();
    stopPlayback = false;
    server.handleClient();
    updateBacklight();
    if (pendingOp != OP_NONE)
        runPendingOp();
    stopPlayback = false;

    if (mjpegCount == 0)
    {
        nowPlaying = "";
        if (!idleScreenShown)
        {
            showInfoScreen("No clips yet");
            idleScreenShown = true;
        }
        delay(5);
        return;
    }

    if (currentMjpegIndex >= mjpegCount)
        currentMjpegIndex = 0;
    playClip(mjpegFileList[currentMjpegIndex]);

    // Finished on its own (or skipped with the button): go to the next clip.
    // Stopped by the phone: runPendingOp() decides what plays next.
    // Stopped by a failed upload: play the same clip again.
    if (pendingOp == OP_NONE && !stopPlayback)
        currentMjpegIndex++;
}

// ------------------------------------------------------------------ playback

int jpegDrawCallback(JPEGDRAW *pDraw)
{
    uint32_t t = millis();
    gfx->draw16bitBeRGBBitmap(pDraw->x, pDraw->y, pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
    draw_ms += millis() - t;
    return 1;
}

void playClip(const String &fileName)
{
    String path = clipPath(fileName);
    Serial.printf("Playing %s\n", path.c_str());
    File mjpegFile = SD.open(path.c_str(), "r");
    if (!mjpegFile || mjpegFile.isDirectory())
    {
        Serial.printf("ERROR: could not open %s\n", path.c_str());
        serveFor(500); // do not spin if the card was pulled
        return;
    }

    nowPlaying = fileName;
    gfx->fillScreen(RGB565_BLACK);
    start_ms = millis();
    total_frames = 0;
    shown_frames = 0;
    skipped_frames = 0;
    draw_ms = 0;
    uint32_t read_ms = 0, decode_and_draw_ms = 0;
    int skipRun = 0;

    int fps = fpsFromName(fileName);
    uint32_t frameMs = fps > 0 ? 1000 / fps : 0; // 0 = as fast as possible
    uint32_t nextFrameAt = millis();             // when the next frame is supposed to appear

    mjpeg.setup(&mjpegFile, mjpeg_buf, jpegDrawCallback, true /* useBigEndian */,
                0, 0, gfx->width(), gfx->height(), estimateBufferSize);

    while (!skipRequested && !stopPlayback && mjpegFile.available())
    {
        uint32_t t0 = millis();
        if (!mjpeg.readMjpegBuf())
            break;
        uint32_t t1 = millis();
        read_ms += t1 - t0;
        total_frames++;

        int32_t late = frameMs > 0 ? (int32_t)(t1 - nextFrameAt) : 0;
        if (late > 500)
        {
            nextFrameAt = t1; // a long pause (an upload): carry on from here instead of racing to catch up
            late = 0;
        }
        if (frameMs > 0 && late > (int32_t)frameMs && skipRun < 3)
        {
            // The board is more than a whole frame behind: do not draw this one, so the clip
            // keeps its real speed instead of turning into slow motion. Never more than 3 in a row.
            skipped_frames++;
            skipRun++;
        }
        else
        {
            mjpeg.drawJpg();
            decode_and_draw_ms += millis() - t1;
            shown_frames++;
            skipRun = 0;
        }

        server.handleClient(); // an upload is handled in here; the clip simply pauses meanwhile
        updateBacklight();

        if (frameMs > 0)
        {
            nextFrameAt += frameMs;
            while (!stopPlayback && (int32_t)(nextFrameAt - millis()) > 0)
            {
                server.handleClient();
                updateBacklight();
                delay(1);
            }
        }
    }

    if (mjpeg.frameTooBig())
        Serial.println("Stopped: this clip has a frame too big for the board's memory.");

    if (skipRequested)
    {
        uint32_t now = millis();
        if (now - lastPress >= BOOT_BUTTON_DEBOUCE_TIME)
            lastPress = now;
    }
    skipRequested = false;

    int time_used = millis() - start_ms;
    mjpegFile.close();
    if (time_used > 0 && shown_frames > 0)
    {
        // "shown per second" is what you actually see. If "skipped" is not 0, the board could not keep up with the target.
        Serial.printf("%d frames in %d ms (target %d fps): shown %d = %0.1f per second, skipped %d\n",
                      total_frames, time_used, fps, shown_frames, 1000.0 * shown_frames / time_used, skipped_frames);
        Serial.printf("   per shown frame: read %0.1f ms, decode %0.1f ms, draw %0.1f ms\n",
                      (float)read_ms / total_frames,
                      (float)(decode_and_draw_ms - draw_ms) / shown_frames,
                      (float)draw_ms / shown_frames);
    }

    if (total_frames == 0)
        serveFor(300); // an empty or broken clip: do not spin
}
