#pragma once
#include "Arduino.h"
#include <map>
extern std::map<std::string, long> g_nvs; // survives a simulated reboot
class Preferences
{
public:
    bool begin(const char *, bool) { return true; }
    bool getBool(const char *k, bool d) { return g_nvs.count(k) ? g_nvs[k] != 0 : d; }
    int getInt(const char *k, int d) { return g_nvs.count(k) ? (int)g_nvs[k] : d; }
    size_t putBool(const char *k, bool v) { g_nvs[k] = v; return 1; }
    size_t putInt(const char *k, int v) { g_nvs[k] = v; return 4; }
};
