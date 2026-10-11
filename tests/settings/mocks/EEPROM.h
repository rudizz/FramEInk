#pragma once
#include <array>
#include <cassert>
struct TestEEPROM
{
    std::array<uint8_t, 512> flash{}, buffer{};
    bool failCommit = false;
    TestEEPROM() { flash.fill(255); buffer = flash; }
    void begin(size_t size) { assert(size == flash.size()); buffer = flash; }
    uint8_t read(int offset) { return buffer.at(offset); }
    void write(int offset, uint8_t byte) { buffer.at(offset) = byte; }
    bool commit() { if (failCommit) return false; flash = buffer; return true; }
};
inline TestEEPROM EEPROM;
