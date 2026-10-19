#include "PhotoUploadService.h"
#include "PortalAssets.h"
#include "../Diagnostics/DiagnosticLog.h"
#include <cstring>
#include <strings.h>
#include <array>
#include <ctime>

namespace {

bool photoName(const char *name)
{
    if (!*name || *name == '.' || strlen(name) > 255)
        return false;
    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(name); *p; ++p)
        if (*p < 32 || *p == 127 || *p == '/' || *p == '\\' || *p == ':')
            return false;
    const char *extension = strrchr(name, '.');
    return extension && (!strcasecmp(extension, ".jpg") || !strcasecmp(extension, ".jpeg") ||
                         !strcasecmp(extension, ".png") || !strcasecmp(extension, ".bmp"));
}

String photoId(const char *name)
{
    const char *hex = "0123456789abcdef";
    String id;
    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(name); *p; ++p)
    {
        id += hex[*p >> 4];
        id += hex[*p & 15];
    }
    return id;
}

int hexDigit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

String jsonString(const char *value)
{
    String result = "\"";
    for (const char *p = value; *p; ++p)
    {
        if (*p == '"' || *p == '\\') result += '\\';
        result += *p;
    }
    return result + "\"";
}

struct LocalTimestamp
{
    unsigned year, month, day, hour, minute, second;
};

bool validDate(const LocalTimestamp &value)
{
    if (value.year < 1980 || value.year > 2099 || value.month < 1 || value.month > 12 ||
        value.hour > 23 || value.minute > 59 || value.second > 59)
        return false;
    const unsigned days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = value.year % 4 == 0 && (value.year % 100 != 0 || value.year % 400 == 0);
    return value.day >= 1 && value.day <= days[value.month - 1] + (value.month == 2 && leap ? 1 : 0);
}

bool parseTimestamp(const char *text, LocalTimestamp &value)
{
    if (strlen(text) != 17) return false;
    for (unsigned i = 0; i < 17; ++i)
        if (text[i] < '0' || text[i] > '9') return false;
    const auto number = [text](unsigned start, unsigned count) {
        unsigned result = 0;
        for (unsigned i = start; i < start + count; ++i) result = result * 10 + text[i] - '0';
        return result;
    };
    value = {number(0, 4), number(4, 2), number(6, 2), number(8, 2), number(10, 2), number(12, 2)};
    return validDate(value);
}

bool unpackModified(uint32_t packed, LocalTimestamp &value)
{
    const uint16_t date = packed >> 16, clock = packed & 0xffff;
    value = {unsigned(1980 + (date >> 9)), unsigned((date >> 5) & 15), unsigned(date & 31),
             unsigned(clock >> 11), unsigned((clock >> 5) & 63), unsigned((clock & 31) * 2)};
    return packed != 0 && validDate(value);
}

uint32_t modificationKey(SdFile &file)
{
    uint16_t date = 0, clock = 0;
    if (!file.getModifyDateTime(&date, &clock)) return 0;
    const uint32_t packed = (uint32_t(date) << 16) | clock;
    LocalTimestamp value{};
    return unpackModified(packed, value) ? packed : 0;
}

struct GalleryPhoto
{
    char name[256] = {};
    uint64_t size = 0;
    uint32_t modified = 0;
};

int comparePhotos(const GalleryPhoto &left, const GalleryPhoto &right, bool byDate)
{
    if (byDate && left.modified != right.modified) return left.modified < right.modified ? -1 : 1;
    const int folded = strcasecmp(left.name, right.name);
    return folded != 0 ? folded : strcmp(left.name, right.name);
}

String galleryCursor(const GalleryPhoto &photo)
{
    char date[9];
    snprintf(date, sizeof(date), "%08lx", static_cast<unsigned long>(photo.modified));
    return String(date) + photoId(photo.name);
}

bool parseGalleryCursor(const String &text, GalleryPhoto &photo)
{
    if (text.length() < 10 || text.length() > 518 || text.length() % 2 != 0) return false;
    for (size_t i = 0; i < text.length(); ++i)
    {
        const int digit = hexDigit(text[i]);
        if (digit < 0) return false;
        if (i < 8) photo.modified = (photo.modified << 4) | digit;
        else if (i % 2 == 0) photo.name[(i - 8) / 2] = static_cast<char>(digit << 4);
        else
        {
            photo.name[(i - 8) / 2] |= digit;
            if (photo.name[(i - 8) / 2] == 0) return false;
        }
    }
    LocalTimestamp date{};
    return photoName(photo.name) && (photo.modified == 0 || unpackModified(photo.modified, date));
}

} // namespace

namespace frameink {

void PhotoUploadService::configure(const PhotoUploadTarget &target)
{
    target_ = target;
}

void PhotoUploadService::touch()
{
    lastActivity_ = millis();
}

void PhotoUploadService::begin(WebServer &server)
{
    server_ = &server;
    const char *requestHeaders[] = {"X-FramEInk-Request", "X-FramEInk-Upload-Time"};
    server.collectHeaders(requestHeaders, 2);
    touch();
    const auto page = [this]() {
        serveAsset(portalHtmlGzip, sizeof(portalHtmlGzip), "text/html; charset=utf-8");
    };
    server.on("/", HTTP_GET, page);
    server.on("/foto", HTTP_GET, page);
    server.on("/portal.js", HTTP_GET, [this]() {
        serveAsset(portalJsGzip, sizeof(portalJsGzip), "application/javascript; charset=utf-8");
    });
    server.on("/heic-to.js", HTTP_GET, [this]() {
        serveAsset(heicToGzip, sizeof(heicToGzip), "application/javascript; charset=utf-8");
    });
    server.on("/api/device", HTTP_GET, [this]() {
        touch();
        String body = "{\"name\":\"";
        body += target_.name;
        body += "\",\"width\":" + String(target_.width);
        body += ",\"height\":" + String(target_.height);
        body += ",\"directory\":\"" + String(target_.directory);
        body += "\",\"grayscale\":" + String(target_.grayscale ? "true" : "false");
        body += ",\"sdReady\":" + String(target_.storage != nullptr ? "true" : "false");
        body += ",\"maxPhotoBytes\":" + String(maxPhotoBytes_) + "}";
        server_->sendHeader("Cache-Control", "no-store");
        server_->send(200, "application/json", body);
    });
    server.on("/api/ping", HTTP_POST, [this]() {
        touch();
        server_->send(204);
    });
    server.on("/api/photos", HTTP_POST,
        [this]() { sendUploadResult(); },
        [this]() { handleUpload(); });
    server.on("/api/photos", HTTP_GET, [this]() { listPhotos(); });
    server.on("/api/photo", HTTP_GET, [this]() { servePhoto(); });
    server.on("/api/logs", HTTP_GET, [this]() { serveLogs(); });
    server.on("/api/photos/delete", HTTP_POST, [this]() { deletePhoto(); });
}

bool PhotoUploadService::galleryReady()
{
    touch();
    server_->sendHeader("Cache-Control", "no-store");
    if (uploading_)
        server_->send(409, "text/plain", "Wait until the photo upload is complete.");
    else if (!target_.storage)
        server_->send(503, "text/plain", "SD card unavailable. Insert it and restart FramEInk.");
    else
        return true;
    return false;
}

void PhotoUploadService::listPhotos()
{
    if (!galleryReady()) return;
    const String offsetText = server_->arg("offset");
    size_t offset = 0;
    if (offsetText.length() > 6)
    {
        server_->send(400, "text/plain", "Invalid page offset.");
        return;
    }
    for (size_t i = 0; i < offsetText.length(); ++i)
    {
        if (offsetText[i] < '0' || offsetText[i] > '9')
        {
            server_->send(400, "text/plain", "Invalid page offset.");
            return;
        }
        offset = offset * 10 + offsetText[i] - '0';
    }
    const String sort = server_->arg("sort"), order = server_->arg("order");
    if ((sort.length() && sort != "name" && sort != "modified") ||
        (order.length() && order != "asc" && order != "desc"))
    {
        server_->send(400, "text/plain", "Invalid gallery sort order.");
        return;
    }
    const bool byDate = sort != "name", ascending = order == "asc";
    GalleryPhoto after;
    const String cursor = server_->arg("cursor");
    bool hasAfter = cursor.length() > 0;
    if (hasAfter && !parseGalleryCursor(cursor, after))
    {
        server_->send(400, "text/plain", "Invalid gallery cursor.");
        return;
    }
    const auto before = [byDate, ascending](const GalleryPhoto &left, const GalleryPhoto &right) {
        const int comparison = comparePhotos(left, right, byDate);
        return ascending ? comparison < 0 : comparison > 0;
    };
    // A sorted top-ten window, not a whole-directory catalogue. New clients use
    // a keyset cursor so every batch takes one directory scan, at any depth.
    std::array<GalleryPhoto, photosPerPage_> page;
    size_t total = 0, remaining = 0, count = 0;
    const auto scan = [&]() {
        total = remaining = count = 0;
        SdFile directory;
        if (target_.storage->exists(target_.directory) && !directory.open(target_.directory, O_RDONLY)) return false;
        SdFile file;
        GalleryPhoto photo;
        while (directory.isOpen() && file.openNext(&directory, O_RDONLY))
        {
            const size_t length = file.getName(photo.name, sizeof(photo.name));
            if (file.isFile() && !file.isHidden() && length > 0 && length < sizeof(photo.name) && photoName(photo.name))
            {
                ++total;
                photo.size = file.fileSize();
                photo.modified = modificationKey(file);
                if (!hasAfter || before(after, photo))
                {
                    ++remaining;
                    size_t position = 0;
                    while (position < count && before(page[position], photo)) ++position;
                    if (position < photosPerPage_)
                    {
                        if (count < photosPerPage_) ++count;
                        for (size_t i = count - 1; i > position; --i) page[i] = page[i - 1];
                        page[position] = photo;
                    }
                }
            }
            file.close();
            touch();
            delay(0);
        }
        directory.close();
        return true;
    };
    // Keep offset-only clients compatible, still with bounded RAM. The portal
    // always supplies a cursor after the first batch and avoids these re-scans.
    size_t skip = hasAfter ? 0 : offset;
    do {
        if (!scan())
        {
            server_->send(500, "text/plain", "Unable to read the photo folder on the SD card.");
            return;
        }
        if (!skip) break;
        const size_t step = skip < count ? skip : count;
        if (!step) { count = 0; break; }
        after = page[step - 1];
        hasAfter = true;
        skip -= step;
    } while (true);

    String body = "{\"photos\":[";
    for (size_t i = 0; i < count; ++i)
    {
        const GalleryPhoto &photo = page[i];
        if (i) body += ',';
        char sizeText[24];
        snprintf(sizeText, sizeof(sizeText), "%llu", static_cast<unsigned long long>(photo.size));
        body += "{\"id\":\"" + photoId(photo.name) + "\",\"name\":" + jsonString(photo.name);
        body += ",\"size\":" + String(sizeText) + ",\"modified\":";
        LocalTimestamp date{};
        if (unpackModified(photo.modified, date))
        {
            char text[24];
            snprintf(text, sizeof(text), "%04u-%02u-%02uT%02u:%02u:%02u", date.year, date.month, date.day,
                     date.hour, date.minute, date.second);
            body += jsonString(text);
        }
        else body += "null";
        body += '}';
    }
    const bool more = remaining > count;
    body += "],\"nextOffset\":" + (more ? String(offset + count) : String("null"));
    body += ",\"pageSize\":" + String(photosPerPage_);
    body += ",\"totalPhotos\":" + String(total);
    body += ",\"totalPages\":" + String((total + photosPerPage_ - 1) / photosPerPage_);
    if (more) body += ",\"nextCursor\":" + jsonString(galleryCursor(page[count - 1]).c_str());
    body += '}';
    server_->send(200, "application/json; charset=utf-8", body);
}

bool PhotoUploadService::openRequestedPhoto(SdFile &file, bool writable)
{
    const String id = server_->arg("id");
    char name[256] = {};
    bool valid = id.length() > 0 && id.length() <= 510 && id.length() % 2 == 0;
    for (size_t i = 0; valid && i < id.length(); i += 2)
    {
        const int high = hexDigit(id[i]), low = hexDigit(id[i + 1]);
        valid = high >= 0 && low >= 0 && (high || low); // No embedded NULs.
        if (valid) name[i / 2] = static_cast<char>((high << 4) | low);
    }
    if (!valid || !photoName(name))
    {
        server_->send(400, "text/plain", "Invalid photo selection.");
        return false;
    }
    // Only regular, visible photos in this device's fixed folder are accessible.
    const String path = String(target_.directory) + "/" + name;
    if (!file.open(path.c_str(), writable ? O_RDWR : O_RDONLY) || !file.isFile() || file.isHidden())
    {
        file.close();
        server_->send(404, "text/plain", "Photo not found. Refresh the gallery.");
        return false;
    }
    return true;
}

void PhotoUploadService::servePhoto()
{
    if (!galleryReady()) return;
    SdFile file;
    if (!openRequestedPhoto(file)) return;
    char name[256] = {};
    file.getName(name, sizeof(name));
    const char *extension = strrchr(name, '.');
    const char *type = !strcasecmp(extension, ".png") ? "image/png" :
                       !strcasecmp(extension, ".bmp") ? "image/bmp" : "image/jpeg";
    server_->sendHeader("Connection", "close");
    server_->sendHeader("X-Content-Type-Options", "nosniff");
    server_->setContentLength(file.fileSize());
    server_->send(200, type, "");
    // Stream in bounded chunks: old SD photos may be much larger than RAM.
    auto client = server_->client();
    uint8_t buffer[2048];
    int count;
    while (client.connected() && (count = file.read(buffer, sizeof(buffer))) > 0)
    {
        if (client.write(buffer, count) != static_cast<size_t>(count)) break;
        touch();
        delay(0);
    }
    file.close();
    client.stop();
}

void PhotoUploadService::serveLogs()
{
    if (!galleryReady()) return;
    const String selection = server_->arg("file");
    if (selection.length() != 0 && selection != "current" && selection != "previous")
    {
        server_->send(400, "text/plain", "Choose current or previous log.");
        return;
    }
    // Fixed allowlist: no client-controlled SD path and no log deletion endpoint.
    const bool previous = selection == "previous";
    SdFile file;
    if (!file.open(previous ? previousLogPath : currentLogPath, O_RDONLY) || !file.isFile())
    {
        file.close();
        server_->send(404, "text/plain", "Log not available. Previous log exists only after rotation.");
        return;
    }
    server_->sendHeader("Content-Disposition", previous
        ? "attachment; filename=frameink-previous.log" : "attachment; filename=frameink-current.log");
    server_->sendHeader("Connection", "close");
    server_->sendHeader("X-Content-Type-Options", "nosniff");
    server_->setContentLength(file.fileSize());
    server_->send(200, "text/plain; charset=utf-8", "");
    auto client = server_->client();
    uint8_t buffer[1024];
    int count;
    const uint32_t started = millis();
    while (client.connected() && static_cast<uint32_t>(millis()) - started < 30000 &&
           (count = file.read(buffer, sizeof(buffer))) > 0)
    {
        if (client.write(buffer, count) != static_cast<size_t>(count)) break;
        touch();
        delay(1);
    }
    file.close();
    client.stop();
}

void PhotoUploadService::deletePhoto()
{
    if (!galleryReady()) return;
    // A custom header prevents cross-origin HTML forms from deleting SD photos.
    // Cross-origin scripts cannot send it without a CORS preflight (not allowed).
    if (server_->header("X-FramEInk-Request") != "1")
    {
        server_->send(403, "text/plain", "Delete photos from the FramEInk settings page.");
        return;
    }
    SdFile file;
    if (!openRequestedPhoto(file, true)) return;
    // remove() operates on the validated open file, never a client-supplied path.
    if (!file.remove())
    {
        file.close();
        server_->send(507, "text/plain", "Unable to delete the photo from the SD card.");
        return;
    }
    server_->send(200, "text/plain", "Photo deleted.");
}

void PhotoUploadService::serveAsset(const uint8_t *data, size_t size, const char *contentType)
{
    touch();
    server_->sendHeader("Content-Encoding", "gzip");
    server_->sendHeader("Cache-Control", "no-cache");
    server_->send_P(200, contentType, reinterpret_cast<const char *>(data), size);
    touch();
}

void PhotoUploadService::discardUpload()
{
    if (uploadFile_.isOpen())
        uploadFile_.close();
    if (target_.storage != nullptr && temporaryPath_.length() > 0)
        target_.storage->remove(temporaryPath_.c_str());
    temporaryPath_ = "";
}

void PhotoUploadService::fail(int status, const char *message)
{
    status_ = status;
    error_ = message;
    discardUpload();
}

void PhotoUploadService::handleUpload()
{
    HTTPUpload &upload = server_->upload();
    touch();

    if (upload.status == UPLOAD_FILE_START)
    {
        // Exactly one photo per request; the browser sends a batch sequentially.
        if (fileSeen_)
        {
            fail(400, "Send only one photo per request.");
            return;
        }
        fileSeen_ = true;
        uploading_ = true;
        uploadComplete_ = false;
        savedPath_ = "";
        error_ = "";
        receivedBytes_ = 0;
        status_ = 400;
        uploadTimestamp_[0] = 0;

        if (target_.storage == nullptr)
        {
            fail(503, "SD card unavailable. Insert it and restart FramEInk.");
            return;
        }
        if (upload.type != "image/jpeg")
        {
            fail(415, "Convert the photo to JPEG before uploading it.");
            return;
        }
        const String timestamp = server_->header("X-FramEInk-Upload-Time");
        LocalTimestamp date{};
        if (timestamp.length())
        {
            if (!parseTimestamp(timestamp.c_str(), date))
            {
                fail(400, "Invalid photo upload time.");
                return;
            }
            strcpy(uploadTimestamp_, timestamp.c_str());
        }
        else
        {
            const time_t now = time(nullptr);
            tm fields{};
            gmtime_r(&now, &fields);
            if (fields.tm_year >= 120 && fields.tm_year <= 199)
                strftime(uploadTimestamp_, sizeof(uploadTimestamp_), "%Y%m%d%H%M%S000", &fields);
        }
        if (!target_.storage->exists(target_.directory) &&
            !target_.storage->mkdir(target_.directory))
        {
            fail(507, "Unable to create the photo folder on the SD card.");
            return;
        }

        // Generate names on the device. Never use a path supplied by the phone.
        for (int attempt = 0; attempt < 10; ++attempt)
        {
            char suffix[64];
            snprintf(suffix, sizeof(suffix), "/photo_%s%s%08lx.jpg", uploadTimestamp_,
                uploadTimestamp_[0] ? "_" : "", static_cast<unsigned long>(esp_random()));
            savedPath_ = String(target_.directory) + suffix;
            temporaryPath_ = String(target_.directory) + "/." + (suffix + 1) + ".part";
            if (!target_.storage->exists(savedPath_.c_str()) &&
                uploadFile_.open(temporaryPath_.c_str(), O_RDWR | O_CREAT | O_EXCL))
                return;
            temporaryPath_ = "";
        }
        fail(507, "Unable to create a file on the SD card. Check the available space.");
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (error_.length() > 0 || !uploadFile_.isOpen())
            return;
        if (upload.currentSize > maxPhotoBytes_ - receivedBytes_)
        {
            fail(413, "JPEG too large (maximum 2 MB).");
            return;
        }
        if (uploadFile_.write(upload.buf, upload.currentSize) != upload.currentSize)
        {
            fail(507, "SD card write failed. Check the available space.");
            return;
        }
        receivedBytes_ += upload.currentSize;
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (error_.length() == 0 && uploadFile_.isOpen())
        {
            if (upload.totalSize != receivedBytes_ || !uploadFile_.sync() || !validJpeg())
                fail(422, "Incomplete JPEG or incorrect resolution for this device.");
            else
            {
                // Commit only after the complete multipart request is accepted.
                // A second file or an aborted request must discard this file too.
                uploadComplete_ = true;
            }
        }
    }
    else if (upload.status == UPLOAD_FILE_ABORTED)
    {
        fail(400, "Upload interrupted. Please try again.");
        uploading_ = false;
        fileSeen_ = false; // Aborted requests do not invoke the normal result handler.
    }
}

bool PhotoUploadService::validJpeg()
{
    if (receivedBytes_ < 4 || !uploadFile_.seekSet(receivedBytes_ - 2) ||
        uploadFile_.read() != 0xff || uploadFile_.read() != 0xd9 ||
        !uploadFile_.seekSet(0) || uploadFile_.read() != 0xff || uploadFile_.read() != 0xd8)
        return false;

    // Locate the baseline JPEG frame and check the browser respected the target.
    while (uploadFile_.curPosition() + 4 < receivedBytes_)
    {
        if (uploadFile_.read() != 0xff)
            return false;
        int marker;
        do { marker = uploadFile_.read(); } while (marker == 0xff);
        if (marker < 0 || marker == 0xda || marker == 0xd9)
            return false;
        const int high = uploadFile_.read();
        const int low = uploadFile_.read();
        if (high < 0 || low < 0)
            return false;
        const uint16_t length = (high << 8) | low;
        if (length < 2 || uploadFile_.curPosition() + length - 2 > receivedBytes_)
            return false;
        if (marker == 0xc0)
        {
            if (length < 8 || uploadFile_.read() != 8)
                return false;
            const int heightHigh = uploadFile_.read();
            const int heightLow = uploadFile_.read();
            const int widthHigh = uploadFile_.read();
            const int widthLow = uploadFile_.read();
            return ((heightHigh << 8) | heightLow) == target_.height &&
                   ((widthHigh << 8) | widthLow) == target_.width;
        }
        if (!uploadFile_.seekCur(length - 2))
            return false;
    }
    return false;
}

void PhotoUploadService::sendUploadResult()
{
    touch();
    if (!fileSeen_ || (!uploadComplete_ && error_.length() == 0))
        fail(400, "No JPEG photo received.");
    if (error_.length() == 0)
    {
        LocalTimestamp date{};
        if (uploadTimestamp_[0] && (!parseTimestamp(uploadTimestamp_, date) ||
            !uploadFile_.timestamp(T_CREATE | T_WRITE, date.year, date.month, date.day, date.hour, date.minute, date.second)))
            fail(507, "Unable to save the photo modification date on the SD card.");
    }
    if (error_.length() == 0)
    {
        if (!uploadFile_.rename(savedPath_.c_str()))
            fail(507, "Unable to finish saving the photo to the SD card.");
        else
        {
            uploadFile_.close();
            temporaryPath_ = "";
        }
    }
    if (error_.length() > 0)
        server_->send(status_, "text/plain; charset=utf-8", error_);
    else
        server_->send(201, "application/json", "{\"path\":\"" + savedPath_ + "\"}");
    discardUpload();
    uploading_ = false;
    fileSeen_ = false;
    uploadComplete_ = false;
}

void PhotoUploadService::end()
{
    discardUpload();
    uploading_ = false;
    fileSeen_ = false;
}

} // namespace frameink
