#include "PhotoUploadService.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <iterator>

using namespace frameink;

// Minimal JPEG header fixture: the service checks frame size and completeness,
// not JPEG entropy decoding (that is separately exercised in the browser).
std::vector<uint8_t> jpeg(uint16_t width = 825, uint16_t height = 1200, uint8_t frame = 0xc0) {
    return {0xff,0xd8, 0xff,0xe0,0,4,0,0, 0xff,frame,0,11,8,
            uint8_t(height >> 8),uint8_t(height),uint8_t(width >> 8),uint8_t(width),1,1,0x11,0,
            0xff,0xd9};
}

struct Fixture {
    SdFat storage;
    WebServer server;
    PhotoUploadService service;
    Fixture(bool sdReady = true, bool color = false) {
        PhotoUploadTarget target;
        target.width = color ? 600 : 825;
        target.height = color ? 448 : 1200;
        target.directory = color ? "/landscape" : "/portrait";
        target.storage = sdReady ? &storage : nullptr;
        service.configure(target);
        server.requestHeaders["X-FramEInk-Request"] = "1";
        service.begin(server);
    }
    void receive(std::vector<uint8_t> data = jpeg(), size_t missing = 0) {
        server.event(UPLOAD_FILE_START);
        server.event(UPLOAD_FILE_WRITE, data);
        server.event(UPLOAD_FILE_END, {}, data.size() + missing);
    }
    void rejected(int status) {
        server.result();
        assert(server.status == status);
        assert(storage.files.empty());
        assert(!service.busy());
    }
};

String idFor(const std::string &name) {
    std::string id;
    for (unsigned char byte : name) {
        id += "0123456789abcdef"[byte >> 4];
        id += "0123456789abcdef"[byte & 15];
    }
    return id;
}

void galleryTests() {
    for (bool color : {false, true}) {
        Fixture f(true, color);
        const std::string folder = color ? "/landscape" : "/portrait";
        f.server.call("/api/photos");
        assert(f.server.status == 200 && std::string(f.server.body.c_str()) == "{\"photos\":[],\"nextOffset\":null,\"pageSize\":10,\"totalPhotos\":0,\"totalPages\":0}");
        f.storage.directories.insert(folder);
        for (int i = 0; i < 23; ++i) {
            char name[24];
            snprintf(name, sizeof(name), "/photo%02d.jpg", i);
            f.storage.files[folder + name] = jpeg();
        }
        f.storage.files[folder + "/.unfinished.jpg.part"] = jpeg();
        f.storage.files[folder + "/.hidden.jpg"] = jpeg();
        f.storage.files[folder + "/metadata.txt"] = {};
        f.storage.files[folder + "/private.jpg"] = jpeg();
        f.storage.hidden.insert(folder + "/private.jpg");
        f.storage.directories.insert(folder + "/directory.jpg");
        f.storage.files["/other/foreign.jpg"] = jpeg();
        f.server.call("/api/photos");
        const std::string page = f.server.body.c_str();
        assert(page.find("\"nextOffset\":10") != std::string::npos);
        assert(page.find("\"totalPhotos\":23,\"totalPages\":3") != std::string::npos);
        assert(page.find("photo09.jpg") != std::string::npos && page.find("photo10.jpg") == std::string::npos);
        for (const char *hidden : {"unfinished", "hidden", "metadata", "directory", "foreign", "private"})
            assert(page.find(hidden) == std::string::npos);
        f.server.arguments["offset"] = "10";
        f.server.call("/api/photos");
        assert(std::string(f.server.body.c_str()).find("photo19.jpg") != std::string::npos);
        assert(std::string(f.server.body.c_str()).find("\"nextOffset\":20") != std::string::npos);
        f.server.arguments["offset"] = "20";
        f.server.call("/api/photos");
        assert(std::string(f.server.body.c_str()).find("photo22.jpg") != std::string::npos);
        assert(std::string(f.server.body.c_str()).find("\"totalPhotos\":23,\"totalPages\":3") != std::string::npos);
        assert(std::string(f.server.body.c_str()).find("\"nextOffset\":null") != std::string::npos);
        for (const char *offset : {"-1", "x", "1000000", "1.5"}) {
            f.server.arguments["offset"] = offset;
            f.server.call("/api/photos"); assert(f.server.status == 400);
        }
        f.server.arguments["id"] = idFor("photo00.jpg");
        f.server.call("/api/photo");
        const auto bytes = jpeg();
        assert(f.server.status == 200 && f.server.contentLength == bytes.size());
        assert(f.server.body.length() == bytes.size());
        assert(std::string(f.server.body.c_str(), f.server.body.length()) == std::string(bytes.begin(), bytes.end()));
        f.server.call("/api/photos/delete", HTTP_POST);
        assert(f.server.status == 200 && !f.storage.files.count(folder + "/photo00.jpg"));
        f.server.call("/api/photos/delete", HTTP_POST); assert(f.server.status == 404);
        f.server.arguments["id"] = idFor("photo01.jpg");
        f.storage.failRemove = true;
        f.server.call("/api/photos/delete", HTTP_POST);
        assert(f.server.status == 507 && f.storage.files.count(folder + "/photo01.jpg"));
        f.storage.failRemove = false;
        const auto saved = f.storage.files;
        for (const std::string &name : std::vector<std::string>{"../other/foreign.jpg", "/other/foreign.jpg", "a\\b.jpg", ".hidden.jpg", "metadata.txt", "volume:photo.jpg", std::string("photo\0.jpg", 10)}) {
            f.server.arguments["id"] = idFor(name);
            f.server.call("/api/photos/delete", HTTP_POST); assert(f.server.status == 400);
            f.server.call("/api/photo"); assert(f.server.status == 400);
        }
        for (const char *id : {"", "zz", "1", "00", "6162ff00"}) {
            f.server.arguments["id"] = id;
            f.server.call("/api/photos/delete", HTTP_POST); assert(f.server.status == 400);
        }
        for (const char *name : {"directory.jpg", "private.jpg", "absent.png"}) {
            f.server.arguments["id"] = idFor(name);
            f.server.call("/api/photos/delete", HTTP_POST); assert(f.server.status == 404);
        }
        assert(f.storage.files == saved); // Invalid selections never mutate any files.
        f.server.requestHeaders.clear();
        f.server.arguments["id"] = idFor("photo01.jpg");
        f.server.call("/api/photos/delete", HTTP_POST);
        assert(f.server.status == 403 && f.storage.files == saved);
        f.server.requestHeaders["X-FramEInk-Request"] = "1";
        f.server.arguments["offset"] = "0";
        f.receive(color ? jpeg(600,448) : jpeg());
        f.server.call("/api/photos"); assert(f.server.status == 409);
        f.server.call("/api/photos/delete", HTTP_POST); assert(f.server.status == 409);
        f.server.result(); assert(f.server.status == 201);
    }
    for (int count : {0, 1, 9, 10, 11, 19, 20, 21}) {
        Fixture f;
        f.storage.directories.insert("/portrait");
        for (int i = 0; i < count; ++i) f.storage.files["/portrait/" + std::to_string(i) + ".jpg"] = jpeg();
        f.server.call("/api/photos");
        const std::string body = f.server.body.c_str();
        assert(body.find("\"totalPhotos\":" + std::to_string(count) + ",\"totalPages\":" + std::to_string((count + 9) / 10)) != std::string::npos);
        size_t entries = 0, position = 0;
        while ((position = body.find("\"id\":", position)) != std::string::npos) { entries++; position++; }
        assert(entries == static_cast<size_t>(std::min(10, count)));
        // Deletion across the 11 -> 10 boundary must update the page total.
        if (count == 11) {
            f.server.arguments["id"] = idFor("0.jpg");
            f.server.call("/api/photos/delete", HTTP_POST); assert(f.server.status == 200);
            f.server.call("/api/photos");
            assert(std::string(f.server.body.c_str()).find("\"totalPhotos\":10,\"totalPages\":1") != std::string::npos);
        }
    }
    { Fixture f(false);
      for (const char *route : {"/api/photos", "/api/photo"}) { f.server.call(route); assert(f.server.status == 503); }
      f.server.call("/api/photos/delete", HTTP_POST); assert(f.server.status == 503); }
    { Fixture f;
      f.storage.directories.insert("/portrait"); f.storage.failOpen = true;
      f.server.call("/api/photos"); assert(f.server.status == 500); }
    { Fixture f;
      f.storage.directories.insert("/portrait");
      f.storage.files["/portrait/Vacanza è \"bella\".PNG"] = jpeg();
      f.storage.files["/portrait/old.bmp"] = {};
      f.storage.files["/portrait/old.jpeg"] = jpeg();
      f.server.call("/api/photos");
      const std::string body = f.server.body.c_str();
      assert(body.find("Vacanza è \\\"bella\\\".PNG") != std::string::npos);
      assert(body.find("old.bmp") != std::string::npos && body.find("old.jpeg") != std::string::npos);
      f.server.arguments["id"] = idFor("Vacanza è \"bella\".PNG");
      f.server.call("/api/photo"); assert(f.server.status == 200);
      f.server.call("/api/photos/delete", HTTP_POST); assert(f.server.status == 200); }
    std::cout << "Photo gallery: listing, pagination, preview, deletion, path safety and SD failures passed.\n";
}

int main(int argc, char **argv) {
    galleryTests();
    for (bool color : {false, true}) {
        Fixture f(true, color);
        const auto data = color ? jpeg(600,448) : jpeg();
        f.receive(data);
        assert(f.service.busy());
        assert(f.storage.files.size() == 1);
        assert(f.storage.files.begin()->first.find(".part") != std::string::npos);
        f.server.result();
        assert(f.server.status == 201 && !f.service.busy());
        const auto &saved = *f.storage.files.begin();
        assert(saved.second == data);
        assert(saved.first.find(color ? "/landscape/photo_" : "/portrait/photo_") == 0);
        assert(saved.first.find(".part") == std::string::npos);
        assert(saved.first.find("..") == std::string::npos);
        // Repeated uploads must not overwrite an existing photo.
        f.receive(data); f.server.result();
        assert(f.server.status == 201 && f.storage.files.size() == 2);
    }
    { Fixture f(false); f.receive(); f.rejected(503); }
    { Fixture f; f.server.currentUpload.type = "image/heic"; f.receive(); f.rejected(415); }
    { Fixture f; f.receive(jpeg(600,448)); f.rejected(422); }
    { Fixture f; f.receive(jpeg(825,1200,0xc2)); f.rejected(422); }
    { Fixture f; auto data = jpeg(); data.pop_back(); f.receive(data); f.rejected(422); }
    { Fixture f; f.receive(jpeg(), 1); f.rejected(422); }
    { Fixture f; auto data = jpeg(); data[10] = 0xff; f.receive(data); f.rejected(422); }
    { Fixture f; f.server.result(); assert(f.server.status == 400); }
    { Fixture f; f.storage.failMkdir = true; f.receive(); f.rejected(507); }
    { Fixture f; f.storage.failOpen = true; f.receive(); f.rejected(507); }
    { Fixture f; f.storage.failWrite = true; f.receive(); f.rejected(507); }
    { Fixture f; f.storage.failSync = true; f.receive(); f.rejected(422); }
    { Fixture f; f.storage.failRename = true; f.receive(); f.rejected(507); }
    { Fixture f; f.receive(std::vector<uint8_t>(2 * 1024 * 1024 + 1)); f.rejected(413); }
    { Fixture f; f.receive(); f.server.event(UPLOAD_FILE_START); f.rejected(400); }
    for (bool afterEnd : {false, true}) {
        Fixture f;
        if (afterEnd) f.receive();
        else { f.server.event(UPLOAD_FILE_START); f.server.event(UPLOAD_FILE_WRITE, jpeg()); }
        f.server.event(UPLOAD_FILE_ABORTED);
        assert(f.storage.files.empty() && !f.service.busy());
        // The next request succeeds even when the aborted request has no result callback.
        f.receive(); f.server.result(); assert(f.server.status == 201);
    }
    { Fixture f; f.receive(); f.service.end(); assert(f.storage.files.empty() && !f.service.busy()); }
    // Optional real browser-generated JPEGs, in order: 10-inch, 6 Color.
    if (argc == 3) {
        for (int i = 1; i <= 2; ++i) {
            std::ifstream file(argv[i], std::ios::binary);
            assert(file.good());
            std::vector<uint8_t> data{std::istreambuf_iterator<char>(file), {}};
            Fixture f(true, i == 2);
            f.receive(data); f.server.result(); assert(f.server.status == 201);
        }
        std::cout << "Real browser JPEGs accepted for both devices.\n";
    }
    std::cout << "PhotoUploadService: all tests passed (both resolutions, SD errors, validation, aborts, atomic commit).\n";
}
