// SDPhoto.h

#ifndef _SDPHOTO_h
#define _SDPHOTO_h

#include <Arduino.h>
#include <Inkplate.h>
#include "Configuration/PhotoOrder.h"
#include "Photos/PhotoSequenceState.h"

class SDPhotoClass
{
 protected:
	 Inkplate *display;
	 bool getFilePath(
		 char* filePath,
		 size_t filePathSize,
		 const char* dirName,
		 frameink::PhotoSequenceState &sequence,
		 frameink::PhotoOrder order,
		 uint& availableFileCount);
	 void showError(const char* message, const char* dirName = nullptr);
	 bool DEBUG_PRINT = false;

 public:
	 SDPhotoClass();
	 SDPhotoClass(Inkplate *_display);
	 enum class PhotoOrientation { landscape, portrait };
	 bool initOk = false;

	void drawImageFromSD(int x, int y, PhotoOrientation orientation, frameink::PhotoSequenceState &sequence,
		frameink::PhotoOrder order = frameink::defaultPhotoOrder);

};

//extern SDPhotoClass SDPhoto;

#endif
