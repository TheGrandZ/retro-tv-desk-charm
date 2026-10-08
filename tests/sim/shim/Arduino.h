// Host-side stand-in for the Arduino core, only for simulating the sketch logic on a PC.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cctype>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#define PROGMEM
#define IRAM_ATTR
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
#define FALLING 2
#define MALLOC_CAP_DMA 0
#define MALLOC_CAP_8BIT 0
#define VSPI 3
typedef const char *PGM_P;
typedef bool boolean;
extern uint32_t g_ms;
inline uint32_t millis() { return g_ms; }
inline void delay(uint32_t ms) { g_ms += ms; }
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
#define ADC_0db 0
extern int g_bl, g_bl_writes;
inline int analogRead(int) { return 0; }
inline void analogWrite(int, int v) { g_bl = v; g_bl_writes++; }
inline void analogSetPinAttenuation(int, int) {}
inline int digitalPinToInterrupt(int p) { return p; }
inline void attachInterrupt(int, void (*)(), int) {}
inline bool isDigit(int c) { return isdigit(c); }
inline bool isAlphaNumeric(int c) { return isalnum(c); }
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
inline void *heap_caps_aligned_alloc(size_t, size_t n, int) { return malloc(n); }
inline void *heap_caps_malloc(size_t n, int) { return malloc(n); }
class String
{
public:
    std::string s;
    String() {}
    String(const char *c) : s(c ? c : "") {}
    String(const std::string &c) : s(c) {}
    String(char c) : s(1, c) {}
    String(int v) : s(std::to_string(v)) {}
    String(unsigned int v) : s(std::to_string(v)) {}
    String(long v) : s(std::to_string(v)) {}
    String(unsigned long v) : s(std::to_string(v)) {}
    unsigned int length() const { return s.size(); }
    const char *c_str() const { return s.c_str(); }
    bool endsWith(const String &o) const { return s.size() >= o.s.size() && s.compare(s.size() - o.s.size(), o.s.size(), o.s) == 0; }
    bool startsWith(const String &o) const { return s.compare(0, o.s.size(), o.s) == 0; }
    int lastIndexOf(char c) const { auto p = s.rfind(c); return p == std::string::npos ? -1 : (int)p; }
    int lastIndexOf(const String &o) const { auto p = s.rfind(o.s); return p == std::string::npos ? -1 : (int)p; }
    int indexOf(char c) const { auto p = s.find(c); return p == std::string::npos ? -1 : (int)p; }
    String substring(unsigned int a) const { return a >= s.size() ? String("") : String(s.substr(a)); }
    String substring(unsigned int a, unsigned int b) const { return a >= s.size() ? String("") : String(s.substr(a, b - a)); }
    long toInt() const { return atol(s.c_str()); }
    void trim() { while (!s.empty() && isspace((unsigned char)s.back())) s.pop_back(); size_t i = 0; while (i < s.size() && isspace((unsigned char)s[i])) i++; s = s.substr(i); }
    char operator[](unsigned int i) const { return i < s.size() ? s[i] : 0; }
    String &operator+=(const String &o) { s += o.s; return *this; }
    String &operator+=(char c) { s += c; return *this; }
    bool operator==(const String &o) const { return s == o.s; }
    bool operator!=(const String &o) const { return s != o.s; }
};
inline String operator+(const String &a, const String &b) { return String(a.s + b.s); }
inline String operator+(const String &a, const char *b) { return String(a.s + b); }
inline String operator+(const char *a, const String &b) { return String(std::string(a) + b.s); }
struct SerialT
{
    void begin(int) {}
    void println(const char *t) { printf("    [serial] %s\n", t); }
    void printf(const char *f, ...) { va_list a; va_start(a, f); ::printf("    [serial] "); vprintf(f, a); va_end(a); }
};
extern SerialT Serial;
struct EspT { unsigned getFreeHeap() { return 150000; } };
extern EspT ESP;
class Stream
{
public:
    virtual ~Stream() {}
    virtual int available() = 0;
    virtual size_t readBytes(uint8_t *buf, size_t n) = 0;
};
class SPIClass { public: SPIClass(int) {} void begin(int, int, int, int) {} };
