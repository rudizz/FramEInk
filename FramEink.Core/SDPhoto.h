// SDPhoto.h

#ifndef _SDPHOTO_h
#define _SDPHOTO_h

#include "arduino.h"
#include <Inkplate.h>
#include <SdFat.h>

class SDPhotoClass
{
 protected:
	 Inkplate *display;
	 bool getFilePath(char* filePath, const char* dirName, uint& counter);

 public:
	 SDPhotoClass();
	 SDPhotoClass(Inkplate *_display);
	 enum class PhotoOrientation { landscape, portrait };
	 bool initOk = false;

	void drawImageFromSD(int x, int y, PhotoOrientation orientation, uint &counter);

};

//extern SDPhotoClass SDPhoto;

#endif
