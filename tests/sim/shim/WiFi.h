#pragma once
#include "Arduino.h"
enum { WIFI_STA, WIFI_AP };
enum wl_status_t { WL_IDLE_STATUS, WL_CONNECTED, WL_DISCONNECTED };
struct IPAddress { std::string a; String toString() const { return String(a); } };
extern bool g_sta_ok;
struct WiFiT
{
    bool sta = false;
    bool mode(int) { return true; }
    bool setHostname(const char *) { return true; }
    wl_status_t begin(const char *, const char *) { sta = true; return WL_IDLE_STATUS; }
    wl_status_t status() { return (sta && g_sta_ok) ? WL_CONNECTED : WL_DISCONNECTED; }
    IPAddress localIP() { return IPAddress{"192.168.1.77"}; }
    IPAddress softAPIP() { return IPAddress{"192.168.4.1"}; }
    bool softAP(const char *, const char *) { return true; }
    bool setSleep(bool) { return true; }
    bool disconnect(bool) { sta = false; return true; }
};
extern WiFiT WiFi;
