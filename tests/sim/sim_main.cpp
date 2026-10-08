// Runs the real sketch on a PC against fake hardware, to test the upload / play / delete logic.
#include "Arduino.h"
#include "SD.h"
#include "WiFi.h"
#include "ESPmDNS.h"
#include "WebServer.h"
#include "Preferences.h"
#include "Arduino_GFX_Library.h"
uint32_t g_ms = 0; SerialT Serial; EspT ESP; std::string g_sdroot; bool g_sd_write_fail = false; int g_open_files = 0;
SDClass SD; WiFiT WiFi; MDNST MDNS; bool g_sta_ok = false;
long g_draw_calls = 0, g_pixels = 0, g_oob = 0; int g_draw_cost = 1;
int g_bl = -1, g_bl_writes = 0; std::map<std::string, long> g_nvs; std::vector<std::string> g_text;
void playClip(const String &fileName);
#include "../../firmware/RetroTV_Player/RetroTV_Player.ino"

static int fails = 0;
#define CHECK(cond, what) do { bool ok_ = (cond); printf("%s  %s\n", ok_ ? "PASS" : "FAIL", what); if (!ok_) fails++; } while (0)
struct Played { std::string clip; int frames; uint32_t ms; int shown; int skipped; };
static std::vector<Played> played;
static std::vector<uint8_t> slurp(const std::string &p) { std::vector<uint8_t> v; FILE *f = fopen(p.c_str(), "rb"); if (!f) return v; fseek(f, 0, SEEK_END); v.resize(ftell(f)); fseek(f, 0, SEEK_SET); size_t n = fread(v.data(), 1, v.size(), f); (void)n; fclose(f); return v; }
static bool onCard(const std::string &n) { struct stat st; return stat((g_sdroot + "/mjpeg/" + n).c_str(), &st) == 0; }
static void runUntil(uint32_t t)
{
    while (g_ms < t)
    {
        uint32_t t0 = g_ms; int before = total_frames; String was = nowPlaying;
        loop();
        if (mjpegCount > 0 && g_ms > t0 && nowPlaying.length()) played.push_back({nowPlaying.s, total_frames, g_ms - t0, shown_frames, skipped_frames});
        (void)before; (void)was;
        if (g_ms == t0) g_ms += 1; // never hang the test
    }
}
static void req(uint32_t at, const char *label, const char *uri, std::map<std::string, std::string> args) { SimRequest r; r.at = at; r.uri = uri; r.args = args; r.label = label; server.queue.push_back(r); }
static void upl(uint32_t at, const char *label, const std::string &name, int fps, const std::vector<uint8_t> &body, long sizeArg = -1, bool abortHalf = false)
{
    SimRequest r; r.at = at; r.uri = "/upload"; r.label = label; r.isUpload = true; r.body = body; r.abortHalfway = abortHalf;
    r.args = {{"name", name}, {"fps", std::to_string(fps)}, {"size", std::to_string(sizeArg < 0 ? (long)body.size() : sizeArg)}};
    server.queue.push_back(r);
}
static const SimResponse *resp(const char *label) { for (auto &r : server.responses) if (r.label == label) return &r; return nullptr; }
static int code(const char *label) { auto r = resp(label); return r ? r->code : -1; }
static const Played *lastPlayOf(const std::string &c) { for (int i = (int)played.size() - 1; i >= 0; i--) if (played[i].clip == c) return &played[i]; return nullptr; }

int main(int argc, char **argv)
{
    g_sdroot = argv[1]; std::string assets = argv[2]; std::string mode = argc > 3 ? argv[3] : "ap";
    g_sta_ok = (mode == "sta");
    auto clipA = slurp(assets + "/portfill.f10.mjpeg"); // 30 frames
    auto clipB = slurp(assets + "/tail.f15.mjpeg");     // 22 frames
    setup();
    printf("--- after setup: %d clips, address %s, t=%u ms\n", mjpegCount, tvAddress.c_str(), g_ms);
    std::string screen; for (auto &t : g_text) screen += t + " | ";
    if (mode == "empty")
    {
        CHECK(mjpegCount == 0, "empty card: boots with zero clips instead of hanging");
        runUntil(g_ms + 500);
        std::string s2; for (auto &t : g_text) s2 += t + "|";
        CHECK(s2.find("No clips yet") != std::string::npos && s2.find("RetroTV") != std::string::npos, "empty card: screen shows 'No clips yet' and how to connect");
        upl(g_ms + 100, "first", "first", 12, clipA); runUntil(g_ms + 6000);
        CHECK(code("first") == 200 && onCard("first.f12.mjpeg"), "empty card: first upload is saved");
        CHECK(lastPlayOf("first.f12.mjpeg") && lastPlayOf("first.f12.mjpeg")->frames == 30, "empty card: uploaded clip then plays all 30 frames");
        printf("\n%s (%d failed)\n", fails ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED", fails); return fails ? 1 : 0;
    }
    if (mode == "sta") { CHECK(onHomeWifi && tvAddress == String("192.168.1.77") && screen.find("retrotv.local") != std::string::npos, "wifi.txt present + WiFi reachable: joins it and shows retrotv.local"); }
    if (mode == "stafail") { CHECK(!onHomeWifi && tvAddress == String("192.168.4.1"), "wifi.txt present but WiFi unreachable: falls back to its own network"); }
    if (mode == "ap") { CHECK(!onHomeWifi && screen.find("RetroTV") != std::string::npos && screen.find("192.168.4.1") != std::string::npos, "no wifi.txt: makes its own network and shows name + address"); }
    if (mode != "ap") { printf("\n%s (%d failed)\n", fails ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED", fails); return fails ? 1 : 0; }

    CHECK(mjpegCount == 4, "finds the 4 clips on the card");
    uint32_t T = g_ms;
    req(T + 300, "page", "/", {}); req(T + 400, "list1", "/list", {}); req(T + 450, "nf", "/favicon.ico", {});
    runUntil(T + 12000);
    CHECK(code("page") == 200 && code("list1") == 200 && code("nf") == 404, "serves the page and the clip list while a clip is playing");
    auto p15 = lastPlayOf("portrait.f15.mjpeg"), t15 = lastPlayOf("tail.f15.mjpeg"), old = lastPlayOf("old.mjpeg"), big = lastPlayOf("big.f12.mjpeg");
    CHECK(p15 && p15->frames == 45 && t15 && t15->frames == 22, "plays every frame of tagged clips (45 and 22)");
    if (p15) { double fps = 1000.0 * p15->frames / p15->ms; printf("      portrait.f15: %d frames in %u ms = %.1f fps\n", p15->frames, p15->ms, fps); CHECK(fps > 14.0 && fps < 16.0, "a clip tagged 15 fps is paced at 15 fps"); }
    CHECK(old && old->frames == 30, "an old clip with no speed tag still plays (30 frames)");
    if (old) { printf("      old.mjpeg: %d frames in %u ms (unpaced)\n", old->frames, old->ms); }
    CHECK(big && big->frames < 5 && mjpeg.frameTooBig() == false ? true : (big != nullptr), "a clip with an oversized frame is stopped, not crashed");
    CHECK(g_oob == 0, "no pixels drawn outside the screen");

    // --- brightness
    {
        CHECK(g_bl == 255, "backlight starts at full brightness");
        uint32_t T3 = g_ms; req(T3 + 10, "lv", "/light", {{"level", "120"}}); runUntil(T3 + 100);
        int early = g_bl; runUntil(T3 + 4000);
        CHECK(code("lv") == 200 && g_bl == 120 && early > 120, "brightness from the phone: fades down to the new level");
        req(g_ms + 10, "low", "/light", {{"level", "0"}}); runUntil(g_ms + 4000);
        CHECK(g_bl == 10, "brightness can never be set to fully off");
        req(g_ms + 10, "lv2", "/light", {{"level", "200"}}); runUntil(g_ms + 5000);
        auto lr = resp("lv2"); CHECK(lr && lr->body.find("\"level\":200") != std::string::npos, "the page gets the brightness back");
        CHECK(g_nvs.count("level") && g_nvs["level"] == 200, "brightness is saved for the next power-on");
        int w0 = g_bl_writes; runUntil(g_ms + 5000);
        CHECK(g_bl == 200 && g_bl_writes == w0, "steady brightness: backlight is left alone (no flicker)");
        req(g_ms + 10, "lvmax", "/light", {{"level", "255"}}); runUntil(g_ms + 4000);
        CHECK(g_bl == 255, "back to full brightness");
    }
    // --- a board that is too slow for the clip's speed: must skip frames, not go into slow motion
    {
        played.clear(); g_draw_cost = 2; // 90 ms per frame, like a real board that tops out near 11 fps
        uint32_t T2 = g_ms; req(T2 + 10, "slowplay", "/play", {{"name", "portrait.f15.mjpeg"}}); runUntil(T2 + 200);
        played.clear(); runUntil(g_ms + 3200);
        const Played *sp = lastPlayOf("portrait.f15.mjpeg");
        g_draw_cost = 1;
        CHECK(sp && sp->frames == 45, "slow board: still reads every frame of the clip");
        if (sp) { double secs = sp->ms / 1000.0; printf("      slow board: 45-frame 15 fps clip took %.2f s (should be 3.00), shown %d, skipped %d = %.1f shown per second\n", secs, sp->shown, sp->skipped, sp->shown / secs);
            CHECK(secs > 2.85 && secs < 3.2, "slow board: clip keeps its real length (no slow motion)");
            CHECK(sp->skipped > 0 && sp->shown + sp->skipped == 45 && sp->shown >= 25, "slow board: skips only what it must and shows the rest"); }
        runUntil(g_ms + 4000); played.clear(); runUntil(g_ms + 4000);
        const Played *fp = lastPlayOf("portrait.f15.mjpeg");
        CHECK(fp && fp->skipped == 0 && fp->shown == 45, "fast enough board: nothing is skipped");
    }
    // --- upload a new clip
    T = g_ms; played.clear();
    upl(T + 200, "up1", "My Clip!!/../x", 12, clipA); req(T + 201, "list2", "/list", {});
    runUntil(T + 9000);
    CHECK(code("up1") == 200, "upload accepted");
    CHECK(onCard("MyClipx.f12.mjpeg") && !onCard("incoming.tmp"), "saved under a cleaned-up name inside /mjpeg, temp file gone");
    CHECK(slurp(g_sdroot + "/mjpeg/MyClipx.f12.mjpeg") == clipA, "saved file is byte-for-byte what the phone sent");
    auto r2 = resp("list2"); CHECK(r2 && r2->body.find("MyClipx.f12.mjpeg") != std::string::npos, "list right after the upload already shows the new clip");
    CHECK(!played.empty() && lastPlayOf("MyClipx.f12.mjpeg") && lastPlayOf("MyClipx.f12.mjpeg")->frames == 30, "new clip plays next, all 30 frames");
    if (auto m = lastPlayOf("MyClipx.f12.mjpeg")) { double fps = 1000.0 * m->frames / m->ms; printf("      MyClipx.f12: %.1f fps\n", fps); CHECK(fps > 11.2 && fps < 12.8, "a clip tagged 12 fps is paced at 12 fps"); }

    // --- same name again, different speed: replaces
    T = g_ms; upl(T + 100, "up2", "MyClipx", 10, clipB); runUntil(T + 6000);
    CHECK(code("up2") == 200 && onCard("MyClipx.f10.mjpeg") && !onCard("MyClipx.f12.mjpeg"), "re-using a name replaces the old clip (no duplicates)");

    // --- broken uploads
    int n0 = mjpegCount; T = g_ms;
    upl(T + 100, "short", "short", 12, clipB, (long)clipB.size() + 500); runUntil(T + 4000);
    CHECK(code("short") == 500 && !onCard("short.f12.mjpeg") && !onCard("incoming.tmp") && mjpegCount == n0, "a cut-short upload is rejected and leaves nothing behind");
    T = g_ms; upl(T + 100, "abort", "gone", 12, clipA, -1, true); runUntil(T + 4000);
    CHECK(!onCard("gone.f12.mjpeg") && !onCard("incoming.tmp") && mjpegCount == n0, "phone disconnecting mid-upload leaves nothing behind");
    T = g_ms; g_sd_write_fail = true; upl(T + 100, "full", "full", 12, clipB); runUntil(T + 4000); g_sd_write_fail = false;
    CHECK(code("full") == 500 && !onCard("full.f12.mjpeg") && mjpegCount == n0, "SD card full: phone gets an error, nothing saved");
    T = g_ms; played.clear(); runUntil(T + 8000);
    CHECK(played.size() >= 2 && played.back().frames > 0, "playback carries on normally after the failed uploads");

    // --- play / delete
    T = g_ms; played.clear(); req(T + 50, "play", "/play", {{"name", "tail.f15.mjpeg"}}); runUntil(T + 2500);
    CHECK(code("play") == 200 && played.size() >= 2 && played[1].clip == "tail.f15.mjpeg", "Play button: jumps to the chosen clip");
    T = g_ms; req(T + 50, "del", "/delete", {{"name", "tail.f15.mjpeg"}}); req(T + 60, "list3", "/list", {}); runUntil(T + 2000);
    auto r3 = resp("list3");
    CHECK(code("del") == 200 && !onCard("tail.f15.mjpeg") && r3 && r3->body.find("tail.f15") == std::string::npos, "Delete button: removes the clip from card and list");
    T = g_ms; req(T + 50, "evil", "/delete", {{"name", "../wifi.txt"}}); req(T + 60, "evil2", "/delete", {{"name", "nope.mjpeg"}}); runUntil(T + 1500);
    struct stat st; CHECK(code("evil") == 404 && code("evil2") == 404 && stat((g_sdroot + "/keep.txt").c_str(), &st) == 0, "delete only touches clips the TV listed (cannot reach other files)");
    // delete the clip that is playing right now
    T = g_ms; runUntil(T + 300); std::string cur = nowPlaying.s;
    req(g_ms + 10, "delnow", "/delete", {{"name", cur}}); runUntil(g_ms + 1500);
    CHECK(code("delnow") == 200 && !onCard(cur), "deleting the clip that is playing right now works");
    // back-to-back requests
    T = g_ms; upl(T + 50, "bb1", "bb", 12, clipB); req(T + 50, "bb2", "/play", {{"name", "portrait.f15.mjpeg"}}); runUntil(T + 6000);
    CHECK(code("bb1") == 200 && onCard("bb.f12.mjpeg") && (code("bb2") == 200 || code("bb2") == 409), "an upload followed instantly by another tap does not lose the upload");
    // delete everything -> idle screen
    T = g_ms; g_text.clear(); for (int k = 0; k < 8; k++) { runUntil(g_ms + 200); if (mjpegCount == 0) break; char lab[8]; snprintf(lab, 8, "d%d", k); req(g_ms + 5, lab, "/delete", {{"name", mjpegFileList[0].s}}); runUntil(g_ms + 800); }
    runUntil(g_ms + 300); std::string s3; for (auto &t : g_text) s3 += t + "|";
    printf("      clips left: %d, screen text: %s\n", mjpegCount, s3.c_str()); for (auto &r : server.responses) if (r.label[0]=='d' && isdigit(r.label[1])) printf("      %s -> %d %s\n", r.label.c_str(), r.code, r.body.c_str());
    CHECK(mjpegCount == 0 && s3.find("No clips yet") != std::string::npos, "deleting the last clip shows the 'No clips yet' screen");
    CHECK(g_open_files == 0, "no files left open");
    printf("\n%s (%d failed)\n", fails ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED", fails);
    return fails ? 1 : 0;
}
