#pragma once
#include "Arduino.h"
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <memory>
#define FILE_READ "r"
#define FILE_WRITE "w"
extern std::string g_sdroot;
extern bool g_sd_write_fail;
extern int g_open_files;
struct FileImpl { FILE *f = nullptr; DIR *d = nullptr; std::string path, name; bool dir = false; ~FileImpl() { if (f) { fclose(f); g_open_files--; } if (d) closedir(d); } };
class File : public Stream
{
    std::shared_ptr<FileImpl> p;
public:
    File() {}
    File(std::shared_ptr<FileImpl> q) : p(q) {}
    operator bool() const { return p && (p->f || p->d); }
    boolean isDirectory() { return p && p->dir; }
    const char *name() const { return p ? p->name.c_str() : ""; }
    size_t size() const { struct stat st; return (p && stat(p->path.c_str(), &st) == 0) ? st.st_size : 0; }
    void close() { p.reset(); }
    int available() override { if (!p || !p->f) return 0; long c = ftell(p->f); fseek(p->f, 0, SEEK_END); long e = ftell(p->f); fseek(p->f, c, SEEK_SET); return (int)(e - c); }
    size_t readBytes(uint8_t *b, size_t n) override { return (p && p->f) ? fread(b, 1, n, p->f) : 0; }
    size_t write(const uint8_t *b, size_t n) { if (g_sd_write_fail) return 0; return (p && p->f) ? fwrite(b, 1, n, p->f) : 0; }
    String readStringUntil(char t) { std::string o; int c; while (p && p->f && (c = fgetc(p->f)) != EOF && c != t) o += (char)c; return String(o); }
    File openNextFile()
    {
        if (!p || !p->d) return File();
        struct dirent *e;
        while ((e = readdir(p->d)))
        {
            std::string n = e->d_name; if (n == "." || n == "..") continue;
            auto q = std::make_shared<FileImpl>(); q->path = p->path + "/" + n; q->name = n;
            struct stat st; stat(q->path.c_str(), &st); q->dir = S_ISDIR(st.st_mode);
            if (!q->dir) { q->f = fopen(q->path.c_str(), "rb"); g_open_files++; } else q->d = opendir(q->path.c_str());
            return File(q);
        }
        return File();
    }
};
class SDClass
{
    std::string full(const char *p) { return g_sdroot + p; }
public:
    bool begin(int, SPIClass &, long, const char *) { return true; }
    File open(const char *path, const char *mode = FILE_READ)
    {
        auto q = std::make_shared<FileImpl>(); q->path = full(path); std::string s = path; q->name = s.substr(s.rfind('/') + 1);
        struct stat st; bool ex = stat(q->path.c_str(), &st) == 0;
        if (ex && S_ISDIR(st.st_mode)) { q->dir = true; q->d = opendir(q->path.c_str()); return File(q); }
        if (mode[0] == 'r' && !ex) return File();
        q->f = fopen(q->path.c_str(), mode[0] == 'w' ? "wb" : "rb"); if (q->f) g_open_files++;
        return q->f ? File(q) : File();
    }
    File open(const String &path, const char *mode = FILE_READ) { return open(path.c_str(), mode); }
    bool exists(const char *p) { struct stat st; return stat(full(p).c_str(), &st) == 0; }
    bool remove(const char *p) { return ::unlink(full(p).c_str()) == 0; }
    bool remove(const String &p) { return remove(p.c_str()); }
    bool rename(const char *a, const char *b) { return ::rename(full(a).c_str(), full(b).c_str()) == 0; }
    bool mkdir(const char *p) { return ::mkdir(full(p).c_str(), 0755) == 0; }
};
extern SDClass SD;
