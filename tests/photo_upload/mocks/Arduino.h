#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <type_traits>

#define PROGMEM

class String {
    std::string value_;
  public:
    String() = default;
    String(const char *value) : value_(value) {}
    String(std::string value) : value_(std::move(value)) {}
    template<class T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    String(T value) : value_(std::to_string(value)) {}
    const char *c_str() const { return value_.c_str(); }
    size_t length() const { return value_.size(); }
    char operator[](size_t index) const { return value_[index]; }
    String &operator+=(char c) { value_ += c; return *this; }
    String &operator+=(const String &other) { value_ += other.value_; return *this; }
    friend String operator+(const String &a, const String &b) { return a.value_ + b.value_; }
    friend bool operator!=(const String &a, const String &b) { return a.value_ != b.value_; }
};

inline unsigned long millis() { return 1234; }
inline void delay(int) {}
inline uint32_t esp_random() { static uint32_t value = 0; return ++value; }
