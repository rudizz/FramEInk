// 
// 
// 

#include "SDPhoto.h"
#include <SdFat.h>

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
		display->println("SD Card error!");
	}

}

bool SDPhotoClass::getFilePath(char* filePath, const char* dirName, uint& counter) {
	File dir;
	File file;
	char fileName[100]; // abbastanza grande da contenere un possibile nome lungo del file nella SD
	if (dir.open(dirName)) {
		/*error("dir.open failed");*/
	  // Open next file in root.
	  // Warning, openNext starts at the current position of dir so a
	  // rewind may be necessary in your application.
		int contaCicli = -1;
		// If counter == -1 -> take Random file
		int fileCount = 0;
		if (counter == -1) {
			dir.rewindDirectory();
			while (file.openNext(&dir, O_RDONLY)) {
				if (file.size() > 4096 && !file.isHidden()) {
					fileCount++;
				}
				file.close();
			}
			Serial.print("fileCount: ");
			Serial.println(fileCount);
			counter = random(fileCount);

			Serial.print("Random Index: ");
			Serial.println(counter);
		}
		dir.rewindDirectory();
		bool fileFounded = false;
		while (file.openNext(&dir, O_RDONLY)) {
			if (file.size() > 4096 && !file.isHidden()) // Scarto tutti i file che non sono immagini
			{
				++contaCicli;
				if (counter == contaCicli)
				{
					file.getName(fileName, sizeof(fileName));
					counter++;
					fileFounded = true;
					break;
				}
			}
			file.close();
		}
		// Se fileFounded = false vuol dire che sono arrivato in fondo alla lista di immagini
		// quindi resetto il counter.
		if (!fileFounded)
		{
			// Se contaCicli < 0 vuol dire che non sono presenti immagini, quindi esco dal metodo
			if (contaCicli > -1) {
				counter = 0;
				Serial.println("Azzero contatore");
				getFilePath(fileName, dirName, counter);
			} else {
				return false;
			}
		}
		// Creo il filePath
		strcpy(filePath, dirName);
		strcat(filePath, fileName);

		return dir.exists(fileName);
	}
	else {
		return false;
	}
}

void SDPhotoClass::drawImageFromSD(int x, int y, PhotoOrientation orientation, uint &counter)
{
	if (initOk)
	{
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

		char filePath[110];
		bool imageDrawed = false;
		// se il disegno dell'immagine fallisce, passo all'immagine successiva.
		while (!imageDrawed)
		{
			if (getFilePath(filePath, dirName, counter))
			{
				Serial.println(filePath);
				if (display->drawImage(filePath, x, y, true, false))
				{
					Serial.println("Draw Image true");
					imageDrawed = true;
				}
				else {
					Serial.println("Draw Image false");
					counter++;
				}
			}
			else
			{
				Serial.println("Warning: image not found!");
				//Serial.println(new String("Warning: image not found at path: ")->concat(&filePath));
			}
		}
	}

}


SDPhotoClass SDPhoto;

