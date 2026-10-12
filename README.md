FramEInk is a e-paper display controlled by an ESP32.
Every hour it changes the content between 2 frames:
1. Weather + Calendar events + Moon Phase
2. Image loaded from the SD card (random by default, or alphabetical)

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
   Il cursore **Zoom** va dal **50% al 400%** rispetto a **Fit**; i cursori
   **Horizontal position / Vertical position** spostano l'immagine anche in
   modalità foto intera. Nuova foto: zoom 100%, posizione centrata. Il JPEG
   trasferito corrisponde al riquadro di anteprima e mantiene la risoluzione
   del dispositivo, anche con zoom e rotazione.
5. Nella sezione **Photos on your frame** scorri all'interno del riquadro:
   l'elenco si estende automaticamente in blocchi di dieci nomi, senza pagine.
   **Sort photos by** sceglie **Last modified** o **Alphabetical**;
   **Order** inverte il verso (**Descending / Ascending**). Default: ultima
   modifica decrescente, quindi gli ultimi caricamenti per primi. L'ordinamento
   riguarda l'intera SD, non solo i nomi già elencati; cambiarlo torna in cima
   senza perdere la selezione. È indipendente dall'ordine di riproduzione sul display.
   Le immagini vengono scaricate dalla SD una alla volta, solo quando la loro
   anteprima entra nell'area visibile (anche rispetto alla pagina del browser).
   Se scorri via durante un download, questo viene interrotto e può riprendere
   quando la foto torna visibile. Le anteprime già viste restano sul telefono
   come piccoli JPEG compressi, senza riscaricare gli originali dalla SD.
   **View photo** apre l'immagine in grande. Puoi selezionarle singolarmente,
   con **Select loaded** per i nomi già elencati o con **Select all** per tutte
   le foto, anche quelle non ancora elencate, quindi usare
   **Delete selected**. La cancellazione è permanente, richiede conferma e riguarda
   solo la cartella del dispositivo sulla SD, non gli originali sul telefono.
   La selezione rimane attiva scorrendo; **Clear selection** la azzera.
   **Clear loaded** deseleziona solo i nomi già elencati. **Refresh** ricarica
   l'elenco, torna in cima, azzera selezione e cache delle anteprime.
   Il contatore indica quante foto sono elencate rispetto al totale; in caso
   di errore **Retry loading photos** riprova senza perdere l'elenco precedente.
   **Select all** legge solo i nomi: non scarica le immagini non visibili.
   Sono supportati JPEG, PNG e BMP,
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

## Ordine di visualizzazione delle foto

Nei Settings web, **Photo playback → Photo order** seleziona **Random** (default)
oppure **Alphabetical**, poi premi **Save settings**. La scelta persiste dopo
lo spegnimento e vale per entrambi i modelli; non cambia l'ordine della galleria web.
Gli altri Settings già salvati sono mantenuti; sulle versioni precedenti si usa Random.

Alphabetical segue i nomi completi dei file, senza distinguere maiuscole/minuscole
(a parità si usa l'ordine dei byte), e riparte dalla prima foto dopo l'ultima.
La posizione è conservata tra i deep sleep, separatamente per `/portrait` e
`/landscape`; una nuova accensione riparte dalla prima foto. Aggiunte e cancellazioni
non invalidano un indice: si cerca il nome successivo all'ultimo mostrato.
Random sceglie una foto a ogni aggiornamento dedicato alle immagini; può ripetersi.
La selezione scorre solo i nomi, con memoria costante: non carica tutte le foto.
Le immagini illeggibili vengono saltate, con un numero limitato di tentativi.
I nomi assegnati agli upload dal telefono sono generati dal dispositivo: Alphabetical
ordina questi nomi, non il nome originale sul telefono o la data di scatto.

Test del selettore SD reale con file e display simulati, senza modificare una SD:

```sh
clang++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -Itests/photo_order/mocks -IFramEink.Core/src tests/photo_order/photo_order_test.cpp FramEink.Core/src/SDPhoto.cpp -o /tmp/frameink-photo-order-test
/tmp/frameink-photo-order-test
```

## Caratteri del calendario e batteria

Titoli e luoghi degli eventi supportano le lettere dell'Europa occidentale:
`à è é ì ò ù`, `ä ö ü`, `ß ẞ`, `ç ñ`, le rispettive maiuscole e altre lettere
Latin-1; sono inclusi anche `Œ œ Š š Ž ž Ÿ` e `€`. Sono riconosciuti sia UTF-8
sia i vecchi byte Latin-1, e gli accenti combinati comuni (ad esempio `e` più
accento grave diventa `è`). I caratteri non supportati, come le emoji, mantengono
il ripiego testuale esistente. Non è un font Unicode completo.

Il parser converte gli eventi nel set di glifi a byte singolo documentato in
`Calendar/CalendarText.h`: così i limiti di 64 caratteri per il titolo e 63 per
il luogo non tagliano a metà una sequenza UTF-8. Gli eventi usano font dedicati;
i font ASCII delle altre parti dello schermo rimangono invariati. Gli a capo
e i puntini di troncamento sono misurati con i font effettivamente visualizzati.

La schermata iniziale delle istruzioni mostra la batteria in alto a destra,
su entrambi i modelli. Condivide con l'indicatore di Inkplate 10 la scala lineare
`3,70 V = vuota`, `3,93 V = metà`, `4,16 V = piena`; i valori esterni all'intervallo
sono limitati a 0–100%. È una stima dalla tensione, non una misura della capacità.

I font generati sono inclusi nel firmware; non richiedono file sulla SD.
Origine, licenze e hash: [NOTICE.md](FramEink.Core/src/Fonts/calendar-licenses/NOTICE.md).

Verifica dei glifi e dell'invarianza dei caratteri ASCII:

```sh
python3 tools/build_calendar_fonts.py --check
```

Per rigenerarli, estrarre l'archivio GNU indicato nel NOTICE e compilare
`fontconvert` con FreeType, quindi eseguire:

```sh
python3 tools/build_calendar_fonts.py --fontconvert /percorso/fontconvert --freefont-dir /percorso/freefont-20120503
```

Test del parser reale, del layout e della batteria, con sanitizzatori e senza SD:

```sh
clang++ -std=c++17 -Wall -Wextra -Werror -Wno-unknown-pragmas -fsanitize=address,undefined -DARDUINO=10800 -Itests/calendar/mocks -IFramEink.Core/src tests/calendar/calendar_text_test.cpp FramEink.Core/src/CommonFunctions.cpp -o /tmp/frameink-calendar-text-test
/tmp/frameink-calendar-text-test /tmp/frameink-calendar-preview.pgm
```

Il portale si chiude dopo **10 minuti senza attività**; durante conversione e
upload viene mantenuto aperto. Non spegnere la cornice durante la scrittura.
Le foto ricevono nomi univoci generati dal dispositivo e non sovrascrivono quelle
esistenti. Gli upload incompleti vengono scartati; eventuali `.part` rimasti dopo
un'interruzione di alimentazione non vengono mostrati come foto.
I nuovi upload registrano la data locale del telefono nella SD, anche prima
della prima sincronizzazione NTP. FAT conserva la modifica con precisione di
due secondi e senza fuso: il nome include anche i millisecondi per distinguere
caricamenti ravvicinati. Le foto precedenti mantengono la data già sulla SD;
quelle senza una data valida appaiono in fondo con il default. Il portale mostra
la data disponibile su ciascuna scheda. Non viene usata la data di scatto EXIF.
I blocchi ordinati usano un cursore e conservano al massimo dieci metadati nella
RAM della board: una scansione dei nomi per blocco, nessun caricamento anticipato
delle immagini fuori schermo.
Limiti: 40 MiB per originale sul telefono e 2 MiB per JPEG trasferito.
Le HEIC molto grandi possono richiedere tempo e memoria sullo smartphone.

## Connessione Wi-Fi e diagnostica

Avvio, meteo e calendario condividono la stessa attesa della connessione:
la riconnessione automatica del driver resta attiva per i fallimenti reali,
ma l'applicazione non interrompe più scansione, autenticazione o DHCP ogni
pochi secondi. Solo dopo **30 secondi** senza connessione completa viene
forzato un nuovo tentativo. Il limite complessivo è **120 secondi**, poi viene
mantenuto il riavvio di recupero già previsto dal firmware.

Il monitor seriale a 115200 baud mostra il tempo di connessione, IP, RSSI,
canale e, a ogni disconnessione, codice e descrizione del motivo, ad esempio:

```text
[WiFi] Disconnected: reason=201 (NO_AP_FOUND: network not found).
[WiFi] Disconnected: reason=202 (AUTH_FAIL: authentication failed).
```

Questi messaggi non stampano SSID o password. La modifica non nasconde gli
avvisi del driver come `AP has neither DSSS parameter nor HT Information`:
il motivo di disconnessione aiuta a capire se sono legati al problema reale.
La sincronizzazione NTP successiva alla connessione attende una nuova risposta
del server; la presenza di una data già valida non viene scambiata per una nuova
sincronizzazione. Il timeout è di 15 secondi, poi si conserva l'ora disponibile.

Test dell'attesa reale con Wi-Fi/orologio simulati, senza board o rete:
connessione lenta, attesa DHCP, timeout e tentativi, riconnessioni successive,
overflow di `millis()`, tutti i codici di errore e assenza di credenziali nei log.

```sh
clang++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -Itests/wifi/mocks -IFramEink.Core/src tests/wifi/wifi_connection_test.cpp FramEink.Core/src/Connectivity/WiFiConnection.cpp -o /tmp/frameink-wifi-connection-test
/tmp/frameink-wifi-connection-test
```

## Risvegli orari e pausa notturna

Nei Settings web, **Hourly updates → Update minute (00–59)** sceglie il minuto
dei risvegli in ora locale della posizione meteo. Default: **00**. Per esempio,
accendendo alle `09:36` con minuto `00`, dopo il calendario iniziale i risvegli
sono `10:00 → 11:00 → 12:00`; con minuto `45`, il primo è `09:45` se l'avvio è
terminato in tempo. I secondi dell'allarme sono `:00`. Un aggiornamento lento o
una correzione NTP non spostano il minuto: si passa al successivo orario utile,
saltando eventuali scadenze già trascorse. Il valore si salva con **Save settings**
e viene mantenuto anche dopo spegnimento e riaccensione.

Si usa l'RTC esterno delle due board, con allarme su GPIO39, invece del solo
timer RC dell'ESP32. Il calendario sincronizza realmente l'ora via NTP e aggiorna
l'RTC; il risveglio dedicato alla foto legge l'RTC senza accendere il Wi-Fi.
Il refresh e-paper può terminare a secondi diversi: è il **risveglio** a essere
ancorato al minuto. Senza Internet a lungo rimane possibile la deriva fisica
dell'RTC, ma non viene introdotto lo slittamento cumulativo del vecchio calcolo.
Un timer di sicurezza interviene cinque minuti dopo l'allarme; se l'RTC o il suo
allarme non sono verificabili, viene usato il timer di ripiego e segnalato nel log.

Nei Settings web, **Night pause → Pause starts / Pause ends** imposta l'intervallo
in ora locale della posizione meteo. Default: **23:00–05:00**, sei ore.
L'inizio è incluso, la fine esclusa; sono supportati intervalli a cavallo della
mezzanotte. Esempio: minuto `:05` e pausa `23:00–05:00` producono l'ultimo risveglio
alle `22:05` e il successivo alle `05:05`. Con fine `06:15` si riprende alle `07:05`.
L'alternanza foto/calendario prosegue dal punto in cui era stata sospesa.
Orari uguali disabilitano la pausa; altrimenti deve restare almeno un'ora libera
al giorno, perché sia sempre disponibile un risveglio al minuto selezionato.
Il fuso è l'ultimo offset ricevuto dal servizio meteo, come per gli eventi.

Gli orari si salvano con **Save settings** e persistono dopo lo spegnimento.
Estensioni EEPROM versionate con checksum mantengono compatibili i cinque
campi già salvati e la pausa notturna della versione precedente. Se manca il
nuovo campo, il minuto di aggiornamento è `00`, senza cancellare gli altri dati.

Test del pianificatore, allarme/fallback RTC e NTP con hardware/orologio simulati:

```sh
clang++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -Dtime=frameink_test_time -Dsettimeofday=frameink_test_settimeofday -Itests/scheduling/mocks -IFramEink.Core/src tests/scheduling/scheduling_test.cpp FramEink.Core/src/Scheduling/InkplateClock.cpp FramEink.Core/src/Connectivity/TimeSync.cpp -o /tmp/frameink-scheduling-test
/tmp/frameink-scheduling-test
```

Test dell'API Settings reale con EEPROM/server simulati: compatibilità precedente,
validazione, salvataggio/rilettura, errore di commit e disabilitazione pausa:

```sh
clang++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -Itests/settings/mocks -Itests/photo_upload/mocks -IFramEink.Core/src/WiFiAccessPoint tests/settings/settings_test.cpp FramEink.Core/src/WiFiAccessPoint/WiFiAPSettings.cpp FramEink.Core/src/WiFiAccessPoint/PhotoUploadService.cpp -o /tmp/frameink-settings-test
/tmp/frameink-settings-test
```

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

Test della galleria JavaScript (richiede Node.js, nessuna dipendenza esterna):
`node tests/portal_selection_test.cjs`. Usa solo DOM e foto simulati in memoria;
verifica caricamento solo visibile, scorrimento progressivo, cache, annullamento
dei download fuori schermo, ripresa, Select all, conferma e gestione degli errori,
ordinamento nome/data nei due versi, zoom/traslazioni/rotazione e JPEG per entrambi
i modelli.

Per provare la pagina nel browser con un dispositivo simulato:

```sh
python3 tools/preview_photo_portal.py --model 10 --port 8765
python3 tools/preview_photo_portal.py --model color --port 8766 --force-heic
```

Il server di prova salva i JPEG solo in una cartella temporanea e ne verifica
formato e dimensioni. `--force-heic` prova anche il decoder incorporato su Safari,
che altrimenti può leggere HEIC nativamente. Con `--seed-photo /percorso/test.jpg`
crea 23 copie temporanee di una foto per provare miniature e scorrimento progressivo:
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
