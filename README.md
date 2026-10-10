FramEInk is a e-paper display controlled by an ESP32.
Every hour it changes the content between 2 frames:
1. Weather + Calendar events + Moon Phase
2. Random image loaded from the sd card

Weather data is provided by OpenWeather service.
Calendar events are retrieved from a personal Google Calendar.

FramEInk supports 6 inch color and 10 inch grayscale frames.

## Caricamento foto da smartphone via Wi‑Fi

1. Inserisci la SD prima di accendere FramEInk. Spegni e riaccendi la cornice
   per aprire il portale (non serve estrarre la scheda).
2. Sul telefono collegati alla rete **FramEInk** con la password temporanea
   di 8 caratteri mostrata sul display (il minimo supportato da WPA2).
   Cambia a ogni accensione; se necessario dimentica la
   rete memorizzata e riconnettila. È normale che la rete non abbia Internet.
3. Apri **Safari/Chrome** su `http://192.168.4.1/` (o all'indirizzo mostrato).
   Se è la prima configurazione, salva prima le impostazioni Wi‑Fi/meteo.
4. Premi **Select photos**: puoi scegliere più immagini dalla galleria.
   Controlla ciascuna anteprima, scegli foto intera o ritaglio a pieno schermo,
   eventualmente ruotala, poi premi **Upload photo**. Il portale passa alla
   successiva dopo il salvataggio. **Skip photo** non la trasferisce.
5. Nella sezione **Photos on your frame** le miniature si caricano automaticamente,
   una alla volta; **View photo** apre l'immagine in grande. Puoi
   selezionarle singolarmente, con **Select this page** per la pagina corrente
   o con **Select all** per tutte le pagine, quindi usare
   **Delete selected**. La cancellazione è permanente, richiede conferma e riguarda
   solo la cartella del dispositivo sulla SD, non gli originali sul telefono.
   La selezione rimane attiva cambiando pagina; **Clear selection** la azzera.
   **Clear this page** deseleziona solo la pagina corrente. **Refresh** ricarica
   l'elenco e azzera la selezione.
   L'elenco mostra dieci foto per pagina e indica pagina corrente e totale
   nel formato `1\3`; **Previous**, **Next** e **Refresh**
   permettono di sfogliarlo e aggiornarlo. Sono supportati JPEG, PNG e BMP,
   comprese le foto copiate sulla SD prima di usare il portale.
6. Premi **Finish**: la cornice mostra il calendario e va in deep sleep.
   Al risveglio mostra la foto e torna in sleep, mantenendo l'alternanza.

La conversione avviene **sul telefono, prima del trasferimento**, anche senza
Internet. HEIC/HEIF dell'iPhone viene decodificato nativamente quando possibile,
altrimenti con il convertitore incorporato nel firmware. Alla SD arriva sempre
un JPEG ridimensionato, non l'originale HEIC. Gli originali sul telefono non
vengono modificati.

- **10"**: 825 × 1200 pixel, scala di grigi, cartella `/portrait`.
- **6 Color**: 600 × 448 pixel, colori conservati, cartella `/landscape`.

Le dimensioni sono comunicate dal dispositivo usando la risoluzione effettiva
del display dopo la rotazione: non vanno selezionate manualmente sul telefono.
Il display applica il proprio dithering quando legge il JPEG.

Il portale si chiude dopo **10 minuti senza attività**; durante conversione e
upload viene mantenuto aperto. Non spegnere la cornice durante la scrittura.
Le foto ricevono nomi univoci generati dal dispositivo e non sovrascrivono quelle
esistenti. Gli upload incompleti vengono scartati; eventuali `.part` rimasti dopo
un'interruzione di alimentazione non vengono mostrati come foto.
Limiti: 40 MiB per originale sul telefono e 2 MiB per JPEG trasferito.
Le HEIC molto grandi possono richiedere tempo e memoria sullo smartphone.

## Compilazione e verifiche del portale

Usare la partizione **Huge APP (3 MB No OTA/1 MB SPIFFS)**: il decoder HEIC
occupa circa 713 KiB di flash compressa e viene scaricato solo quando necessario.
I file web e il decoder sono già inclusi in `PortalAssets.h`; non occorre copiare
asset sulla SD, avviare servizi esterni o installare pacchetti Python.

Build verificate con Inkplate Boards 2.0.0, InkplateLibrary 11.2.0 e ArduinoJson
7.4.3. La versione 11.2.0 della libreria Inkplate omette `<time.h>` nel proprio
RTC: il flag seguente aggira il problema senza modificare la libreria installata.

La libreria installata su questo computer è stata corretta aggiungendo
`#include <time.h>` in `src/features/rtc/rtc.cpp`: qui il flag non è più necessario,
anche usando Upload da VS Code. Un aggiornamento/reinstallazione della libreria
può richiedere nuovamente la correzione oppure il flag. VS Code è configurato
per **Inkplate 6 Color**, con lo sketch `FramEink_6Color/FramEink_6Color.ino`
e la cartella di build `.build/inkplate6color` (esclusa da Git).
Lo sketch `FramEink/FramEink.ino` è per Inkplate 10: entrambi gli sketch
bloccano la compilazione se la scheda selezionata non corrisponde.

```sh
arduino-cli compile --fqbn Inkplate_Boards:esp32:Inkplate10:PartitionScheme=huge_app --build-property 'compiler.cpp.extra_flags=-include time.h' FramEink
arduino-cli compile --fqbn Inkplate_Boards:esp32:Inkplate6COLOR:PartitionScheme=huge_app --build-property 'compiler.cpp.extra_flags=-include time.h' FramEink_6Color
```

Dopo modifiche a HTML/JS, rigenerare gli asset; `--check` verifica l'allineamento:

```sh
python3 tools/build_portal_assets.py
python3 tools/build_portal_assets.py --check
```

Test del servizio C++ reale con SD e server simulati, senza hardware o scritture
su una SD reale (Clang, AddressSanitizer e UndefinedBehaviorSanitizer):
copre anche elenco paginato, lettura e cancellazione, nomi speciali, errori SD,
protezione dei percorsi e rifiuto delle cancellazioni provenienti da altre pagine.

```sh
clang++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -Itests/photo_upload/mocks -IFramEink.Core/src/WiFiAccessPoint tests/photo_upload/photo_upload_test.cpp FramEink.Core/src/WiFiAccessPoint/PhotoUploadService.cpp -o /tmp/frameink-photo-upload-test
/tmp/frameink-photo-upload-test
```

Test della selezione JavaScript (richiede Node.js, nessuna dipendenza esterna):
`node tests/portal_selection_test.cjs`. Usa solo DOM e foto simulati in memoria;
verifica Select all su più pagine, deselezione, conferma e gestione degli errori.

Per provare la pagina nel browser con un dispositivo simulato:

```sh
python3 tools/preview_photo_portal.py --model 10 --port 8765
python3 tools/preview_photo_portal.py --model color --port 8766 --force-heic
```

Il server di prova salva i JPEG solo in una cartella temporanea e ne verifica
formato e dimensioni. `--force-heic` prova anche il decoder incorporato su Safari,
che altrimenti può leggere HEIC nativamente. Con `--seed-photo /percorso/test.jpg`
crea 23 copie temporanee di una foto per provare miniature e tre pagine:
le operazioni riguardano solo le copie, non il file originale o la SD reale.
Queste verifiche non sostituiscono
il test finale sul dispositivo con una SD reale.

Origine, licenza e istruzioni di sostituzione del decoder:
[web/vendor/README.md](FramEink.Core/web/vendor/README.md).

Color: Weather + Calendar
![color_calendar](https://github.com/user-attachments/assets/82e3d70d-a9f7-4235-be9b-11e1e49212d4)

Color: settings
![color_settings](https://github.com/user-attachments/assets/9c5d514e-a552-4af2-bd68-dd3defbfab2c)

Color: photo
![color_photo](https://github.com/user-attachments/assets/36b21016-b5ec-4551-bd1e-923728857bce)

Grayscale: photo
![grayscale_photo](https://github.com/user-attachments/assets/15bbb233-c09a-4dc5-a1e0-f0137fc78cd2)
