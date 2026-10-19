#pragma once

#include <WebServer.h>
#include "PhotoUploadTarget.h"

namespace frameink {

class PhotoUploadService
{
  public:
    void configure(const PhotoUploadTarget &target);
    void begin(WebServer &server);
    void end();
    void touch();
    bool busy() const { return uploading_; }
    unsigned long lastActivity() const { return lastActivity_; }

  private:
    void handleUpload();
    void sendUploadResult();
    void listPhotos();
    void servePhoto();
    void serveLogs();
    void deletePhoto();
    bool galleryReady();
    bool openRequestedPhoto(SdFile &file, bool writable = false);
    void fail(int status, const char *message);
    void discardUpload();
    bool validJpeg();
    void serveAsset(const uint8_t *data, size_t size, const char *contentType);

    static constexpr size_t maxPhotoBytes_ = 2 * 1024 * 1024;
    static constexpr size_t photosPerPage_ = 10;
    PhotoUploadTarget target_;
    WebServer *server_ = nullptr;
    SdFile uploadFile_;
    String temporaryPath_;
    String savedPath_;
    String error_;
    size_t receivedBytes_ = 0;
    unsigned long lastActivity_ = 0;
    bool uploading_ = false;
    bool fileSeen_ = false;
    bool uploadComplete_ = false;
    char uploadTimestamp_[18] = {}; // Phone-local YYYYMMDDhhmmssSSS; FAT has no timezone.
    int status_ = 400;
};

} // namespace frameink
