#pragma once
struct MDNST { bool begin(const char *) { return true; } bool addService(const char *, const char *, int) { return true; } };
extern MDNST MDNS;
