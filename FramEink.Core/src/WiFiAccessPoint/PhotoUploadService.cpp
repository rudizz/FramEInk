#include "PhotoUploadService.h"
#include "PortalAssets.h"
#include <cstring>
#include <strings.h>

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
    const char *requestHeaders[] = {"X-FramEInk-Request"};
    server.collectHeaders(requestHeaders, 1);
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
    SdFile directory;
    if (target_.storage->exists(target_.directory) && !directory.open(target_.directory, O_RDONLY))
    {
        server_->send(500, "text/plain", "Unable to read the photo folder on the SD card.");
        return;
    }
    String body = "{\"photos\":[";
    size_t index = 0, count = 0;
    SdFile file;
    char name[256] = {};
    while (directory.isOpen() && file.openNext(&directory, O_RDONLY))
    {
        const size_t length = file.getName(name, sizeof(name));
        if (file.isFile() && !file.isHidden() && length > 0 && length < sizeof(name) && photoName(name))
        {
            const size_t photoIndex = index++;
            if (photoIndex >= offset && count < photosPerPage_)
            {
                if (count++) body += ',';
                char sizeText[24];
                snprintf(sizeText, sizeof(sizeText), "%llu", static_cast<unsigned long long>(file.fileSize()));
                body += "{\"id\":\"" + photoId(name) + "\",\"name\":" + jsonString(name);
                body += ",\"size\":" + String(sizeText) + "}";
            }
        }
        file.close();
        touch();
        delay(0);
    }
    directory.close();
    // Count all visible photos, but retain only this page's metadata in RAM.
    body += "],\"nextOffset\":" + (offset + count < index ? String(offset + count) : String("null"));
    body += ",\"pageSize\":" + String(photosPerPage_);
    body += ",\"totalPhotos\":" + String(index);
    body += ",\"totalPages\":" + String((index + photosPerPage_ - 1) / photosPerPage_) + "}";
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
        if (!target_.storage->exists(target_.directory) &&
            !target_.storage->mkdir(target_.directory))
        {
            fail(507, "Unable to create the photo folder on the SD card.");
            return;
        }

        // Generate names on the device. Never use a path supplied by the phone.
        for (int attempt = 0; attempt < 10; ++attempt)
        {
            char suffix[40];
            snprintf(suffix, sizeof(suffix), "/photo_%08lx.jpg", static_cast<unsigned long>(esp_random()));
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
