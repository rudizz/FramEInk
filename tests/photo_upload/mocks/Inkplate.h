#pragma once
#include "Arduino.h"
#include <map>
#include <set>
#include <vector>
#include <cstring>
#include <algorithm>

constexpr int O_RDONLY = 0, O_RDWR = 1, O_CREAT = 2, O_EXCL = 4;
constexpr uint8_t T_CREATE = 2, T_WRITE = 4;
constexpr uint16_t FS_DATE(unsigned year, unsigned month, unsigned day) { return ((year - 1980) << 9) | (month << 5) | day; }
constexpr uint16_t FS_TIME(unsigned hour, unsigned minute, unsigned second) { return (hour << 11) | (minute << 5) | (second / 2); }

// In-memory SdFat double, including injected SD failures. No real files touched.
class SdFat {
  public:
    inline static SdFat *active = nullptr;
    std::map<std::string, std::vector<uint8_t>> files;
    std::set<std::string> directories;
    std::map<std::string, std::pair<uint16_t, uint16_t>> modified;
    bool failMkdir = false, failOpen = false, failWrite = false;
    bool failSync = false, failRename = false, failRemove = false;
    bool failTimestamp = false;
    unsigned directoryScans = 0;
    std::set<std::string> hidden;
    SdFat() { active = this; }
    bool exists(const char *path) { return files.count(path) || directories.count(path); }
    bool mkdir(const char *path) { return !failMkdir && directories.insert(path).second; }
    bool remove(const char *path) {
        if (failRemove || !files.erase(path)) return false;
        modified.erase(path);
        return true;
    }
};

class SdFile {
    SdFat *storage_ = nullptr;
    std::string path_;
    size_t position_ = 0;
    size_t entry_ = 0;
    bool writable_ = false;
  public:
    bool open(const char *path, int flags = O_RDONLY) {
        storage_ = SdFat::active;
        if (storage_->failOpen || ((flags & O_EXCL) && storage_->exists(path))) return false;
        if (!storage_->exists(path)) {
            if (!(flags & O_CREAT)) return false;
            storage_->files[path] = {};
        }
        path_ = path;
        position_ = 0;
        entry_ = 0;
        writable_ = flags & O_RDWR;
        if (storage_->directories.count(path)) ++storage_->directoryScans;
        return true;
    }
    bool openNext(SdFile *directory, int flags = O_RDONLY) {
        std::set<std::string> entries;
        const std::string prefix = directory->path_ + "/";
        auto add = [&](const std::string &path) {
            if (path.find(prefix) == 0 && path.find('/', prefix.size()) == std::string::npos) entries.insert(path);
        };
        for (const auto &item : directory->storage_->files) add(item.first);
        for (const auto &item : directory->storage_->directories) add(item);
        if (directory->entry_ >= entries.size()) return false;
        auto entry = entries.begin();
        std::advance(entry, directory->entry_++);
        return open(entry->c_str(), flags);
    }
    bool isFile() const { return isOpen() && storage_->files.count(path_); }
    bool isHidden() const { return storage_->hidden.count(path_); }
    size_t fileSize() const { return storage_->files.at(path_).size(); }
    bool getModifyDateTime(uint16_t *date, uint16_t *clock) const {
        const auto found = storage_->modified.find(path_);
        if (found == storage_->modified.end()) return false;
        *date = found->second.first;
        *clock = found->second.second;
        return true;
    }
    bool timestamp(uint8_t flags, uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second) {
        if (storage_->failTimestamp || !writable_ || !(flags & T_WRITE)) return false;
        storage_->modified[path_] = {FS_DATE(year, month, day), FS_TIME(hour, minute, second)};
        return true;
    }
    size_t getName(char *name, size_t size) const {
        const auto basename = path_.substr(path_.find_last_of('/') + 1);
        if (basename.size() >= size) { name[0] = 0; return 0; }
        std::memcpy(name, basename.c_str(), basename.size() + 1);
        return basename.size();
    }
    bool isOpen() const { return !path_.empty(); }
    void close() { path_.clear(); }
    size_t write(const uint8_t *data, size_t size) {
        if (storage_->failWrite) return 0;
        auto &bytes = storage_->files.at(path_);
        bytes.insert(bytes.end(), data, data + size);
        position_ += size;
        return size;
    }
    bool sync() { return !storage_->failSync; }
    bool seekSet(size_t position) { position_ = position; return position <= storage_->files.at(path_).size(); }
    bool seekCur(size_t count) { return seekSet(position_ + count); }
    size_t curPosition() { return position_; }
    int read() {
        auto &bytes = storage_->files.at(path_);
        return position_ < bytes.size() ? bytes[position_++] : -1;
    }
    int read(uint8_t *buffer, size_t size) {
        const auto &bytes = storage_->files.at(path_);
        size = std::min(size, bytes.size() - position_);
        std::copy_n(bytes.begin() + position_, size, buffer);
        position_ += size;
        return static_cast<int>(size);
    }
    bool remove() {
        if (!writable_ || !storage_->remove(path_.c_str())) return false;
        close();
        return true;
    }
    bool rename(const char *path) {
        if (storage_->failRename || storage_->exists(path)) return false;
        storage_->files[path] = std::move(storage_->files.at(path_));
        storage_->files.erase(path_);
        if (storage_->modified.count(path_)) {
            storage_->modified[path] = storage_->modified.at(path_);
            storage_->modified.erase(path_);
        }
        path_ = path;
        return true;
    }
};
