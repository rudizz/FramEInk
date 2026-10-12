#pragma once
#include "Arduino.h"
#include <map>
#include <vector>

constexpr int O_RDONLY = 0;
struct TestPhoto
{
    std::string name;
    size_t size = 100;
    bool hidden = false;
    bool directory = false;
    bool decodable = true;
};
struct TestCard
{
    inline static TestCard *active = nullptr;
    std::map<std::string, std::vector<TestPhoto>> folders;
    bool initOk = true, failOpen = false;
    unsigned openHandles = 0, entriesRead = 0;
    TestCard() { active = this; }
};

// Preserve insertion (FAT directory) order, deliberately NOT alphabetical.
class SdFile
{
    TestCard *card_ = nullptr;
    std::vector<TestPhoto> *folder_ = nullptr;
    TestPhoto *photo_ = nullptr;
    size_t next_ = 0;
    bool open_ = false;
  public:
    ~SdFile() { close(); }
    bool open(const char *path)
    {
        close();
        card_ = TestCard::active;
        std::string directory(path);
        if (!directory.empty() && directory.back() == '/') directory.pop_back();
        if (card_->failOpen || !card_->folders.count(directory)) return false;
        folder_ = &card_->folders.at(directory);
        next_ = 0;
        open_ = true;
        ++card_->openHandles;
        return true;
    }
    bool openNext(SdFile *directory, int flags)
    {
        assert(flags == O_RDONLY);
        close();
        if (directory->next_ >= directory->folder_->size()) return false;
        card_ = directory->card_;
        photo_ = &directory->folder_->at(directory->next_++);
        open_ = true;
        ++card_->openHandles;
        ++card_->entriesRead;
        return true;
    }
    bool isFile() const { return photo_ && !photo_->directory; }
    bool isHidden() const { return photo_->hidden; }
    size_t fileSize() const { return photo_->size; }
    size_t getName(char *name, size_t capacity) const
    {
        if (photo_->name.size() >= capacity) { name[0] = 0; return 0; }
        std::memcpy(name, photo_->name.c_str(), photo_->name.size() + 1);
        return photo_->name.size();
    }
    void close()
    {
        if (open_) { assert(card_->openHandles > 0); --card_->openHandles; }
        open_ = false;
        photo_ = nullptr;
    }
};

class Inkplate
{
    TestCard &card_;
  public:
    std::vector<std::string> attempts;
    std::string error;
    explicit Inkplate(TestCard &card) : card_(card) { TestCard::active = &card; }
    int sdCardInit() { return card_.initOk ? 1 : 0; }
    void setCursor(int, int) {}
    void setTextColor(int, int) {}
    void setTextSize(int) {}
    void print(const char *text) { error += text; }
    void println(const char *text = "") { print(text); error += '\n'; }
    void drawBitmap3Bit(int, int, const uint8_t *, int, int) {}
    bool drawImage(const char *path, int, int, bool dither, bool invert)
    {
        assert(dither && !invert);
        attempts.emplace_back(path);
        const std::string full(path);
        const auto split = full.find_last_of('/');
        for (const auto &photo : card_.folders.at(full.substr(0, split)))
            if (photo.name == full.substr(split + 1)) return photo.decodable;
        return false;
    }
};
