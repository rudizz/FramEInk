// 
// 
// 

#include "SDPhoto.h"

SDPhotoClass::SDPhotoClass()
{
}

SDPhotoClass::SDPhotoClass(Inkplate* _display)
{
	display = _display;
	if (display->sdCardInit() > 0)
	{
		initOk = true;
	} else {
		// If SD card init not success, display error on screen
		display->setCursor(30, 30);
		display->setTextColor(0, 7);
		display->println("SD Card error!");
		if (DEBUG_PRINT)
			Serial.println("SD Card error!");
	}

}

bool SDPhotoClass::getFilePath(
	char* filePath,
	size_t filePathSize,
	const char* dirName,
	uint& counter,
	uint& availableFileCount)
{
	if (filePath == nullptr || filePathSize == 0)
		return false;

	filePath[0] = 0;
	availableFileCount = 0;

	File dir;
	File file;
	char fileName[100] = {};

	if (!dir.open(dirName))
		return false;

	dir.rewindDirectory();
	while (file.openNext(&dir, O_RDONLY))
	{
		if (file.size() > 4096 && !file.isHidden())
			++availableFileCount;
		file.close();
	}

	if (DEBUG_PRINT)
		Serial.printf("fileCount: %u\n", availableFileCount);

	if (availableFileCount == 0)
	{
		dir.close();
		return false;
	}

	const uint selectedIndex =
		counter == static_cast<uint>(-1)
			? static_cast<uint>(random(availableFileCount))
			: counter % availableFileCount;

	if (DEBUG_PRINT)
		Serial.printf("Photo index: %u\n", selectedIndex);

	dir.rewindDirectory();
	uint currentIndex = 0;
	bool fileFound = false;
	while (file.openNext(&dir, O_RDONLY))
	{
		const bool validPhoto = file.size() > 4096 && !file.isHidden();
		if (validPhoto && currentIndex == selectedIndex)
		{
			file.getName(fileName, sizeof(fileName));
			fileFound = true;
			file.close();
			break;
		}

		if (validPhoto)
			++currentIndex;
		file.close();
	}
	dir.close();

	if (!fileFound)
		return false;

	const int written = snprintf(filePath, filePathSize, "%s%s", dirName, fileName);
	if (written < 0 || static_cast<size_t>(written) >= filePathSize)
	{
		filePath[0] = 0;
		return false;
	}

	counter = (selectedIndex + 1) % availableFileCount;
	return true;
}

void SDPhotoClass::showError(const char* message, const char* dirName)
{
	display->setCursor(30, 30);
	display->setTextColor(0, 7);
	display->setTextSize(1);
	display->print(message);
	if (dirName != nullptr)
		display->println(dirName);
	else
		display->println();
}

void SDPhotoClass::drawImageFromSD(int x, int y, PhotoOrientation orientation, uint &counter)
{
	if (!initOk)
	{
		showError("SD Card error!");
		return;
	}

	const char *dirName;
	switch (orientation)
	{
	case SDPhotoClass::PhotoOrientation::landscape:
		dirName = "/landscape/";
		break;

	case SDPhotoClass::PhotoOrientation::portrait:
		dirName = "/portrait/";
		break;
	}

	char filePath[112];
	uint availableFileCount = 0;
	uint failedAttempts = 0;

	while (true)
	{
		if (!getFilePath(filePath, sizeof(filePath), dirName, counter, availableFileCount))
		{
			if (DEBUG_PRINT)
				Serial.println("Warning: image not found!");
			showError("No valid photos in ", dirName);
			return;
		}

		if (DEBUG_PRINT)
			Serial.println(filePath);

		if (display->drawImage(filePath, x, y, true, false))
		{
			if (DEBUG_PRINT)
				Serial.println("Draw Image true");
			return;
		}

		if (DEBUG_PRINT)
			Serial.println("Draw Image false");

		++failedAttempts;
		if (failedAttempts >= availableFileCount)
		{
			showError("Unable to draw photos in ", dirName);
			return;
		}
	}
}


SDPhotoClass SDPhoto;
