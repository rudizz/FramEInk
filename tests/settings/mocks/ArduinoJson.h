#pragma once
#include <Arduino.h>
#include <map>
#include <sstream>
#include <iomanip>
struct TestJsonValue
{
    std::string text;
    TestJsonValue &operator=(const String &value) { text = value.c_str(); return *this; }
    TestJsonValue &operator=(const char *value) { text = value; return *this; }
};
struct JsonDocument
{
    std::map<std::string, TestJsonValue> fields;
    TestJsonValue &operator[](const char *key) { return fields[key]; }
};
inline void serializeJson(const JsonDocument &document, String &result)
{
    std::ostringstream output;
    output << '{';
    bool first = true;
    for (const auto &field : document.fields)
    {
        if (!first) output << ',';
        first = false;
        output << std::quoted(field.first) << ':' << std::quoted(field.second.text);
    }
    output << '}';
    result = String(output.str());
}
