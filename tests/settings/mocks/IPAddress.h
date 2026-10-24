#pragma once
#include <Arduino.h>
#include <array>

class IPAddress {
  public:
    IPAddress(uint8_t a = 0, uint8_t b = 0, uint8_t c = 0, uint8_t d = 0) : bytes_{{a, b, c, d}} {}
    uint8_t operator[](size_t index) const { return bytes_.at(index); }
    String toString() const {
        return String(bytes_[0]) + "." + String(bytes_[1]) + "." + String(bytes_[2]) + "." + String(bytes_[3]);
    }
  private:
    std::array<uint8_t, 4> bytes_;
};
