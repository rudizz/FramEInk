# FramEink Core Architecture

## Current layering

- `src/Configuration`
  Contains typed configuration models such as `DeviceConfiguration` and `NetworkConfiguration`.
- `src/WiFiAccessPoint`
  Contains persistence and access-point concerns, plus the `IDeviceConfigurationProvider` interface and `PortalSettingsService` implementation.
- `src/Weather`
  Contains the typed `WeatherForecast` model used by both displays.
- `src/Calendar`
  Contains `CalendarAgenda` and `CalendarService`.
- `src/App`
  Contains the shared `ApplicationController` and renderer-facing runtime contracts.
- `src/Diagnostics`
  Contains bounded SD logs, reset reasons, RTC breadcrumbs and repeated-reset recovery.
  Called only from the main task; Wi-Fi callbacks queue numeric reasons atomically.
- `src`
  Contains platform services such as `Network`, `SDPhoto`, and shared calendar parsing helpers.

## What is modernized in this phase

- WiFi, calendar, and coordinates are now represented by `frameink::DeviceConfiguration`.
- Runtime network state is now represented by `frameink::NetworkConfiguration`.
- `Network` no longer depends on sketch-global `ssid`, `pass`, `calendarURL`, or `timeZone`.
- Sketches obtain configuration through `frameink::IDeviceConfigurationProvider`.
- Weather data is represented by the typed `frameink::WeatherForecast` model under `src/Weather`.
- Calendar download, naming, parsing, and sorting now live behind `frameink::CalendarService` and return a typed `frameink::CalendarAgenda`.
- Shared setup and refresh flow now runs through `frameink::ApplicationController`.
- Each sketch now acts as a thin boot/deep-sleep shell around a display-specific renderer:
  - `FramEink_6Color/FramEink6ColorRenderer.h`
  - `FramEink/FramEink10Renderer.h`

## Resulting flow

1. The sketch boots and restores `frameink::ApplicationRuntime` from RTC memory.
2. `ApplicationController` ensures settings are available through `IDeviceConfigurationProvider`.
3. The controller loads `WeatherForecast` and `CalendarAgenda` using shared core services.
   Requests and parsing have resource budgets; unavailable network data does not restart the device.
4. The display-specific renderer draws either the dashboard or the photo view.
5. The sketch computes deep-sleep timing, records the completed cycle and powers down.
6. Fault resets resume only a valid RTC session whose Settings completed with Wi-Fi credentials;
   a watchdog before application startup opens Settings. Three consecutive failures open a recovery portal,
   then sleep before retrying. Log downloads are available from that portal or a normal power-on.

## Suggested next improvements

1. Move battery/time formatting into a small shared presentation helper if you want even thinner renderers.
2. Replace raw `char` payloads inside `EventClass` with typed value objects when memory budget allows.
3. Add an automated Arduino build step so these architectural refactors can be verified outside the IDE.
