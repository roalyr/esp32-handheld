// [Revision: v1.0] [Path: emulator_mocks/SdFat.h] [Date: 2026-06-09]
// Description: SdFat file system library mock for redirecting SD card IO to the host file system.

#ifndef EMULATOR_SDFAT_H
#define EMULATOR_SDFAT_H

#include "Arduino.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <cstring>

#define O_RDONLY 0x01
#define O_WRONLY 0x02
#define O_RDWR   0x03
#define O_CREAT  0x04
#define O_TRUNC  0x08
#define O_APPEND 0x10
#define O_READ   O_RDONLY
#define O_WRITE  O_WRONLY

#define DEDICATED_SPI 0
#define SD_SCK_MHZ(x) x

class SdSpiConfig {
public:
    SdSpiConfig(int cs, int mode, int speed, void* spi) {}
};

inline std::string mapPath(const std::string& path) {
    std::string mapped = "./emulator_sd";
    if (path.empty() || path[0] != '/') {
        mapped += "/";
    }
    mapped += path;
    return mapped;
}

class FsFile {
public:
    std::fstream stream;
    std::string path;
    DIR* dirp;
    struct dirent* dp;
    bool _isDir;
    std::string dirPath;

    FsFile() : dirp(nullptr), dp(nullptr), _isDir(false) {}

    ~FsFile() {
        close();
    }

    bool open(const char* filepath, int mode) {
        close();
        path = filepath;
        std::string realPath = mapPath(filepath);

        struct stat st;
        if (stat(realPath.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                _isDir = true;
                dirPath = filepath;
                dirp = opendir(realPath.c_str());
                return dirp != nullptr;
            }
        }

        std::ios::openmode openMode = (std::ios::openmode)0;
        if (mode & O_RDONLY) openMode |= std::ios::in;
        if (mode & O_WRONLY) openMode |= std::ios::out;
        if (mode & O_RDWR)   openMode |= (std::ios::in | std::ios::out);
        if (mode & O_TRUNC)  openMode |= std::ios::trunc;
        if (mode & O_APPEND) openMode |= std::ios::app;

        if (mode & O_CREAT) {
            std::ofstream f(realPath, std::ios::app);
            f.close();
        }

        stream.open(realPath, openMode);
        return stream.is_open();
    }

    void close() {
        if (stream.is_open()) {
            stream.close();
        }
        if (dirp) {
            closedir(dirp);
            dirp = nullptr;
        }
        dp = nullptr;
        _isDir = false;
    }

    operator bool() const {
        return stream.is_open() || dirp != nullptr;
    }

    bool isDir() const {
        return _isDir;
    }

    size_t size() {
        if (_isDir) return 0;
        std::string realPath = mapPath(path);
        struct stat st;
        if (stat(realPath.c_str(), &st) == 0) {
            return st.st_size;
        }
        return 0;
    }

    int read() {
        if (!stream.is_open()) return -1;
        return stream.get();
    }

    size_t read(void* buf, size_t count) {
        if (!stream.is_open()) return 0;
        stream.read((char*)buf, count);
        return stream.gcount();
    }

    size_t write(const uint8_t* buf, size_t size) {
        if (!stream.is_open()) return 0;
        stream.write((const char*)buf, size);
        return size;
    }

    size_t write(uint8_t b) {
        if (!stream.is_open()) return 0;
        stream.put(b);
        return 1;
    }

    void print(const String& s) {
        if (stream.is_open()) stream << s.c_str();
    }

    void println(const String& s) {
        if (stream.is_open()) stream << s.c_str() << "\n";
    }

    void print(const char* s) {
        if (stream.is_open() && s) stream << s;
    }

    void println(const char* s) {
        if (stream.is_open() && s) stream << s << "\n";
    }

    bool seek(uint32_t pos) {
        if (!stream.is_open()) return false;
        stream.clear();
        stream.seekg(pos);
        stream.seekp(pos);
        return true;
    }

    bool seekSet(uint32_t pos) {
        return seek(pos);
    }

    uint32_t position() {
        if (!stream.is_open()) return 0;
        return (uint32_t)stream.tellg();
    }

    int available() {
        if (!stream.is_open()) return 0;
        int cur = stream.tellg();
        stream.seekg(0, std::ios::end);
        int end = stream.tellg();
        stream.seekg(cur);
        return end - cur;
    }

    void rewind() {
        if (dirp) {
            rewinddir(dirp);
        }
    }

    bool openNext(FsFile* dir, int mode) {
        if (!dir || !dir->dirp) return false;
        close();
        
        while (true) {
            struct dirent* d = readdir(dir->dirp);
            if (!d) return false;
            if (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0) {
                continue;
            }
            std::string parentPath = dir->dirPath;
            if (parentPath.back() != '/') parentPath += "/";
            std::string childPath = parentPath + d->d_name;
            return open(childPath.c_str(), mode);
        }
    }

    size_t getName(char* buf, size_t size) {
        size_t lastSlash = path.find_last_of('/');
        std::string name = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);
        strncpy(buf, name.c_str(), size);
        return name.length();
    }

    bool sync() {
        if (stream.is_open()) {
            stream.flush();
        }
        return true;
    }

    bool getModifyDateTime(uint16_t* date, uint16_t* time) {
        if (date) *date = 0;
        if (time) *time = 0;
        return true;
    }

    bool getError() {
        return false;
    }
};

class SdFat {
public:
    bool begin(const SdSpiConfig& config) { return true; }
    void end() {}
    int sdErrorCode() { return 0; }
    int sdErrorData() { return 0; }
    uint32_t clusterCount() { return 1000; }
    uint32_t sectorsPerCluster() { return 8; }
    uint32_t freeClusterCount() { return 500; }

    bool exists(const char* filepath) {
        std::string realPath = mapPath(filepath);
        struct stat st;
        return stat(realPath.c_str(), &st) == 0;
    }

    bool remove(const char* filepath) {
        std::string realPath = mapPath(filepath);
        return unlink(realPath.c_str()) == 0;
    }

    bool mkdir(const char* dirpath) {
        std::string realPath = mapPath(dirpath);
        return ::mkdir(realPath.c_str(), 0777) == 0;
    }

    bool rmdir(const char* dirpath) {
        std::string realPath = mapPath(dirpath);
        return ::rmdir(realPath.c_str()) == 0;
    }
};

extern SdFat sdFat;

#endif // EMULATOR_SDFAT_H
