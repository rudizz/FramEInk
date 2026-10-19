#include <App/ApplicationController.h>
#include <cassert>
#include <iostream>

bool connected = true;
unsigned connectionAttempts = 0;
namespace frameink {
bool ensureWiFiConnected(const char *, const char *, uint32_t) { ++connectionAttempts; return connected; }
bool synchronizeNetworkTime(uint32_t) { return true; }
}
struct Settings : frameink::IDeviceConfigurationProvider {
    frameink::DeviceConfiguration config;
    unsigned portals = 0;
    void beginPortal() override { ++portals; }
    void waitUntilConfigured() override {}
    frameink::DeviceConfiguration currentConfiguration() const override { return config; }
    frameink::DeviceConfiguration ensureLoaded(bool &ok) override { ok = true; return config; }
    const char *accessPointSsid() const override { return "test"; }
    IPAddress accessPointIp() const override { return {}; }
    uint16_t accessPointDurationMinutes() const override { return 10; }
};
struct Renderer : frameink::IApplicationRenderer {
    unsigned photos = 0, dashboards = 0, portals = 0;
    bool hasCalendar = false;
    void renderSettingsPortal(const frameink::IDeviceConfigurationProvider &) override { ++portals; }
    void renderSettingsSaved() override {}
    void renderPhoto(frameink::ApplicationRuntime &, frameink::PhotoOrder) override { ++photos; }
    void renderDashboard(const frameink::DashboardModel &model, frameink::ApplicationRuntime &) override {
        ++dashboards;
        hasCalendar = model.hasCalendar;
    }
};
const std::string calendar = "BEGIN:VCALENDAR\r\nX-WR-CALNAME:Test\r\nEND:VCALENDAR\r\n";
int main() {
    using namespace frameink;
    NetworkConfiguration config;
    config.device.wifiSsid = "test-network";
    config.device.calendarUrl = "https://example.test/calendar.ics";
    Network network(&config);
    CalendarService service(&network);
    CalendarAgenda agenda;
    char buffer[100];
    connected = false;
    assert(!network.begin());
    WeatherForecast weather;
    assert(!network.loadWeatherForecast(weather));
    assert(network.getDataCalendar(buffer, sizeof(buffer)) == 0);
    assert(connectionAttempts == 1 && requests == 0);
    connected = true;
    assert(network.begin());
    responses.push_back({200, calendar, -1, {}, {}});
    assert(service.loadAgenda(agenda, 0, 6, 1792022400));
    assert(std::string(agenda.name) == "Test");
    for (int code : {301, 401, 403, 404, 500, -1}) {
        responses.push_back({code, "", 0, {}, {}});
        const unsigned before = requests;
        assert(!service.loadAgenda(agenda, 0, 6, 1792022400));
        assert(requests == before + 1 && !agenda.available && agenda.eventCount == 0);
    }
    for (const std::string &body : {std::string("short"), std::string("BEGIN:VCALENDAR\r\n"), std::string("<html>error</html>")}) {
        responses.push_back({200, body, -1, {}, {}});
        assert(network.getDataCalendar(buffer, sizeof(buffer)) == 0);
    }
    responses.push_back({200, calendar, 2000000, {}, {}});
    assert(!service.loadAgenda(agenda, 0, 6, 1792022400));
    responses.push_back({200, calendar, -1, "chunked", {}});
    assert(network.getDataCalendar(buffer, sizeof(buffer)) == 0);
    responses.push_back({200, calendar, -1, {}, "gzip"});
    assert(network.getDataCalendar(buffer, sizeof(buffer)) == 0);
    failPsram = true;
    const unsigned before = requests;
    assert(!service.loadAgenda(agenda, 0, 6, 1792022400) && requests == before);
    failPsram = false;

    Settings settings;
    settings.config = config.device;
    Renderer renderer;
    ApplicationController controller(&network, &settings, &service, {});
    ApplicationRuntime runtime{};
    runtime.settingsOk = true;
    runtime.stateCalendar = false;
    auto result = controller.executeCycle(runtime, renderer);
    assert(result.mode == RefreshMode::Photo && renderer.photos == 1 && runtime.stateCalendar);
    connected = false;
    result = controller.executeCycle(runtime, renderer);
    assert(result.mode == RefreshMode::Dashboard && renderer.dashboards == 1 && !runtime.stateCalendar);
    assert(settings.portals == 0 && !renderer.hasCalendar);
    connected = true;
    runtime.stateCalendar = true;
    responses.push_back({200, R"({"timezone_offset":3600,"daily":[{}]})", -1, {}, {}});
    responses.push_back({200, calendar, -1, {}, {}});
    result = controller.executeCycle(runtime, renderer);
    assert(renderer.hasCalendar && runtime.timeZoneSeconds == 3600 && !runtime.stateCalendar);
    runtime.settingsOk = false;
    const unsigned oldRequests = requests, oldDashboards = renderer.dashboards;
    result = controller.executeCycle(runtime, renderer, true);
    assert(!result.shouldPresent && settings.portals == 1 && renderer.portals == 1);
    assert(requests == oldRequests && renderer.dashboards == oldDashboards);
    assert(responses.empty());
    std::cout << "Real Network/CalendarService/Controller: bounded errors, PSRAM failure, offline photo-calendar cycle, recovery portal passed.\n";
}
