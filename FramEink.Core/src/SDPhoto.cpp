// 
// 
// 

#include "SDPhoto.h"
#include "InkplateImageCompat.h"
#include <strings.h>
#include "Diagnostics/DiagnosticLog.h"

namespace {

bool isPhoto(SdFile &file, char (&name)[frameink::photoFileNameSize])
{
    if (!file.isFile() || file.fileSize() == 0 || file.isHidden())
        return false;
    if (file.getName(name, sizeof(name)) == 0 || name[0] == '.')
        return false;
    const char *extension = strrchr(name, '.');
    return extension != nullptr &&
        (strcasecmp(extension, ".jpg") == 0 || strcasecmp(extension, ".jpeg") == 0 ||
         strcasecmp(extension, ".png") == 0 || strcasecmp(extension, ".bmp") == 0);
}

int comparePhotoNames(const char *left, const char *right)
{
    const int folded = strcasecmp(left, right);
    // Stable strict order even for names that differ only in case.
    return folded != 0 ? folded : strcmp(left, right);
}

} // namespace

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
	frameink::PhotoSequenceState &sequence,
	frameink::PhotoOrder order,
	uint& availableFileCount)
{
	if (filePath == nullptr || filePathSize == 0)
		return false;

	filePath[0] = 0;
	availableFileCount = 0;

	SdFile dir;
	SdFile file;
	char fileName[frameink::photoFileNameSize] = {};
	char selected[frameink::photoFileNameSize] = {};
	char first[frameink::photoFileNameSize] = {};

	if (!dir.open(dirName))
		return false;

	while (file.openNext(&dir, O_RDONLY))
	{
        if (isPhoto(file, fileName))
        {
            ++availableFileCount;
            if (order != frameink::PhotoOrder::Alphabetical)
            {
                // Reservoir sampling: one SD scan and constant RAM, with the
                // same chance for every photo regardless of directory order.
                if (random(availableFileCount) == 0)
                    strcpy(selected, fileName);
            }
            else
            {
                if (first[0] == 0 || comparePhotoNames(fileName, first) < 0)
                    strcpy(first, fileName);
                if (comparePhotoNames(fileName, sequence.lastFileName) > 0 &&
                    (selected[0] == 0 || comparePhotoNames(fileName, selected) < 0))
                    strcpy(selected, fileName);
            }
        }
		file.close();
        delay(1);
	}
	dir.close();

	if (DEBUG_PRINT)
		Serial.printf("fileCount: %u\n", availableFileCount);

	if (availableFileCount == 0)
	{
		return false;
	}

    if (selected[0] == 0)
    {
        strcpy(selected, first); // Alphabetical wrap after the last file.
    }
	const int written = snprintf(filePath, filePathSize, "%s%s", dirName, selected);
	if (written < 0 || static_cast<size_t>(written) >= filePathSize)
	{
		filePath[0] = 0;
		return false;
	}

	strcpy(sequence.lastFileName, selected);
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

void SDPhotoClass::drawImageFromSD(int x, int y, PhotoOrientation orientation,
    frameink::PhotoSequenceState &sequence, frameink::PhotoOrder order)
{
	if (!initOk)
	{
		showError("SD Card error!");
		return;
	}

	const char *dirName = "/landscape/";
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
    sequence.lastFileName[sizeof(sequence.lastFileName) - 1] = 0;

	while (true)
	{
		if (!getFilePath(filePath, sizeof(filePath), dirName, sequence, order, availableFileCount))
		{
			if (DEBUG_PRINT)
				Serial.println("Warning: image not found!");
			showError("No valid photos in ", dirName);
			return;
		}

		if (DEBUG_PRINT)
			Serial.println(filePath);

        frameink::diagnosticStage("photo_decode");
		if (frameink::drawImage(*display, filePath, x, y, true, false))
		{
			if (DEBUG_PRINT)
				Serial.println("Draw Image true");
            frameink::diagnosticLog("Photo decoded successfully");
			return;
		}

		if (DEBUG_PRINT)
			Serial.println("Draw Image false");

		++failedAttempts;
        frameink::diagnosticLog("Photo decode failed attempt=%u", failedAttempts);
        // After a random first choice, visit the remaining files in name order
        // so a corrupt photo cannot cause retries of the same file indefinitely.
        order = frameink::PhotoOrder::Alphabetical;
		if (failedAttempts >= availableFileCount || failedAttempts >= 8)
		{
			showError("Unable to draw photos in ", dirName);
			return;
		}
	}
}


SDPhotoClass SDPhoto;
