#include "SDPhoto.h"
#include <array>
#include <cassert>
#include <iostream>
#include <random>
#include <set>
#include <type_traits>

using namespace frameink;
std::function<long(long)> testRandom;
static_assert(std::is_trivial<PhotoSequenceState>::value, "Do not reset the retained photo cursor at wake");

namespace {
using Orientation = SDPhotoClass::PhotoOrientation;

void next(Inkplate &display, PhotoSequenceState &sequence, PhotoOrder order,
          Orientation orientation = Orientation::landscape)
{
    // A fresh object on every call models rebooting after deep sleep.
    SDPhotoClass renderer(&display);
    renderer.drawImageFromSD(0, 0, orientation, sequence, order);
    assert(TestCard::active->openHandles == 0);
}

void alphabeticalTests()
{
    TestCard card;
    card.folders["/landscape"] = {{"z.jpg"}, {"B.PNG"}, {"a.jpg"}, {"b.jpeg"}, {"10.jpg"}, {"2.jpg"},
        {".hidden.jpg"}, {"hidden.jpg", 100, true}, {"zero.jpg", 0}, {"notes.txt"},
        {"folder.jpg", 100, false, true}, {std::string(100, 'x') + ".jpg"}};
    Inkplate display(card);
    PhotoSequenceState sequence{};
    testRandom = [](long) -> long { assert(false && "Alphabetical must not call random"); return 0; };
    for (const char *expected : {"10.jpg", "2.jpg", "a.jpg", "b.jpeg", "B.PNG", "z.jpg", "10.jpg"})
    {
        const auto before = card.entriesRead;
        next(display, sequence, PhotoOrder::Alphabetical);
        assert(display.attempts.back() == std::string("/landscape/") + expected);
        assert(std::string(sequence.lastFileName) == expected);
        assert(card.entriesRead - before == card.folders["/landscape"].size()); // One scan, independent of rank.
    }

    // Each orientation has its own retained cursor.
    card.folders["/portrait"] = {{"z.jpg"}, {"a.jpg"}};
    PhotoSequenceState portrait{};
    next(display, portrait, PhotoOrder::Alphabetical, Orientation::portrait);
    assert(display.attempts.back() == "/portrait/a.jpg");
    next(display, sequence, PhotoOrder::Alphabetical);
    assert(display.attempts.back() == "/landscape/2.jpg");
    next(display, portrait, PhotoOrder::Alphabetical, Orientation::portrait);
    assert(display.attempts.back() == "/portrait/z.jpg");

    // Deleting the last photo or inserting a name doesn't invalidate an index.
    card.folders["/landscape"] = {{"c.jpg"}, {"a.jpg"}, {"b.jpg"}};
    sequence = {};
    next(display, sequence, PhotoOrder::Alphabetical);
    assert(display.attempts.back() == "/landscape/a.jpg");
    card.folders["/landscape"] = {{"c.jpg"}, {"aa.jpg"}, {"b.jpg"}};
    for (const char *expected : {"aa.jpg", "b.jpg", "c.jpg", "aa.jpg"})
    {
        next(display, sequence, PhotoOrder::Alphabetical);
        assert(display.attempts.back() == std::string("/landscape/") + expected);
    }
    sequence = {}; // A new power-on starts from the first name.
    next(display, sequence, PhotoOrder::Alphabetical);
    assert(display.attempts.back() == "/landscape/aa.jpg");
}

void randomAndFailureTests()
{
    TestCard card;
    card.folders["/landscape"] = {{"c.jpg"}, {"a.jpg"}, {"b.jpg"}};
    Inkplate display(card);
    PhotoSequenceState sequence{};
    std::mt19937 generator(12345);
    testRandom = [&](long maximum) { return std::uniform_int_distribution<long>(0, maximum - 1)(generator); };
    std::map<std::string, unsigned> counts;
    for (unsigned i = 0; i < 6000; ++i)
    {
        next(display, sequence, PhotoOrder::Random);
        ++counts[display.attempts.back()];
    }
    assert(counts.size() == 3);
    for (const auto &count : counts) assert(count.second > 1750 && count.second < 2250);

    card.folders["/landscape"] = {{"only.jpg"}};
    next(display, sequence, PhotoOrder::Alphabetical);
    assert(display.attempts.back() == "/landscape/only.jpg");
    next(display, sequence, PhotoOrder::Random);
    assert(display.attempts.back() == "/landscape/only.jpg");

    card.folders["/landscape"] = {{"b.jpg"}, {"a.jpg", 100, false, false, false}};
    sequence = {};
    display.attempts.clear();
    next(display, sequence, PhotoOrder::Alphabetical);
    assert((display.attempts == std::vector<std::string>{"/landscape/a.jpg", "/landscape/b.jpg"}));
    testRandom = [](long) { return 0; }; // Reservoir always picks the last entry, a.jpg.
    display.attempts.clear();
    next(display, sequence, PhotoOrder::Random);
    assert((display.attempts == std::vector<std::string>{"/landscape/a.jpg", "/landscape/b.jpg"}));

    // All corrupt: every photo is tried exactly once, including random fallback.
    card.folders["/landscape"] = {{"c.jpg", 100, false, false, false}, {"a.jpg", 100, false, false, false},
        {"b.jpg", 100, false, false, false}};
    for (PhotoOrder order : {PhotoOrder::Random, PhotoOrder::Alphabetical})
    {
        sequence = {};
        display.attempts.clear();
        display.error.clear();
        next(display, sequence, order);
        assert(display.attempts.size() == 3);
        assert(std::set<std::string>(display.attempts.begin(), display.attempts.end()).size() == 3);
        assert(display.error.find("Unable to draw") != std::string::npos);
    }
    card.folders["/landscape"].clear();
    display.attempts.clear();
    display.error.clear();
    next(display, sequence, PhotoOrder::Random);
    assert(display.attempts.empty() && display.error.find("No valid photos") != std::string::npos);
    card.failOpen = true;
    next(display, sequence, PhotoOrder::Alphabetical);
    assert(display.attempts.empty());
    card.initOk = false;
    display.error.clear();
    next(display, sequence, PhotoOrder::Alphabetical);
    assert(display.attempts.empty() && display.error.find("SD Card error") != std::string::npos);
}

struct Storage
{
    std::array<uint8_t, 512> bytes;
    Storage() { bytes.fill(42); }
    uint8_t read(int offset) { return bytes.at(offset); }
    void write(int offset, uint8_t value) { bytes.at(offset) = value; }
};
void storageTests()
{
    PhotoOrder parsed = PhotoOrder::Alphabetical;
    for (const char *bad : {"", "Random", "ALPHABETICAL", "date", "0", " random"})
        assert(!parsePhotoOrder(bad, parsed) && parsed == PhotoOrder::Alphabetical);
    assert(!parsePhotoOrder(nullptr, parsed));
    assert(parsePhotoOrder("random", parsed) && parsed == PhotoOrder::Random);
    assert(parsePhotoOrder("alphabetical", parsed) && parsed == PhotoOrder::Alphabetical);
    for (int offset : {0, 50, 300, 507})
    {
        Storage storage;
        const auto before = storage.bytes;
        assert(readPhotoOrder(storage, offset, 512) == PhotoOrder::Random);
        assert(writePhotoOrder(storage, offset, 512, PhotoOrder::Alphabetical));
        assert(readPhotoOrder(storage, offset, 512) == PhotoOrder::Alphabetical);
        for (int i = 0; i < 512; ++i)
            if (i < offset || i >= offset + photoOrderStorageBytes) assert(storage.bytes[i] == before[i]);
        const auto valid = storage.bytes;
        for (int index = 0; index < photoOrderStorageBytes; ++index)
            for (unsigned bit = 0; bit < 8; ++bit)
            {
                storage.bytes = valid;
                storage.bytes[offset + index] ^= 1 << bit;
                assert(readPhotoOrder(storage, offset, 512) == PhotoOrder::Random);
            }
        assert(writePhotoOrder(storage, offset, 512, PhotoOrder::Random));
        assert(readPhotoOrder(storage, offset, 512) == PhotoOrder::Random);
    }
    Storage storage;
    const auto before = storage.bytes;
    assert(!writePhotoOrder(storage, -1, 512, PhotoOrder::Alphabetical));
    assert(!writePhotoOrder(storage, 508, 512, PhotoOrder::Alphabetical));
    assert(!writePhotoOrder(storage, 0, 512, static_cast<PhotoOrder>(2)));
    assert(storage.bytes == before && readPhotoOrder(storage, 508, 512) == PhotoOrder::Random);
}
} // namespace

int main()
{
    alphabeticalTests();
    randomAndFailureTests();
    storageTests();
    std::cout << "Photo order: alphabetical/wrap/retained cursors, random, SD/decode failures and storage passed\n";
}
