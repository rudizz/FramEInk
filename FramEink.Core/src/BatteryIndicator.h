#pragma once

#include <cmath>
#include <cstdint>

namespace frameink {

constexpr float emptyBatteryVoltage = 3.70f;
constexpr float fullBatteryVoltage = 4.16f;

inline float batteryLevel(float voltage)
{
    if (!std::isfinite(voltage) || voltage <= emptyBatteryVoltage)
        return 0.0f;
    if (voltage >= fullBatteryVoltage)
        return 1.0f;
    return (voltage - emptyBatteryVoltage) / (fullBatteryVoltage - emptyBatteryVoltage);
}

template<class Display>
void drawBatteryIcon(Display &display, int x, int y, float voltage, uint16_t color)
{
    constexpr int width = 34, height = 16, terminalWidth = 4, terminalHeight = 8;
    display.drawRect(x, y, width, height, color);
    display.fillRect(x + width, y + (height - terminalHeight) / 2, terminalWidth, terminalHeight, color);
    const int fill = static_cast<int>(std::round(batteryLevel(voltage) * (width - 4)));
    if (fill > 0)
        display.fillRect(x + 2, y + 2, fill, height - 4, color);
}

} // namespace frameink
