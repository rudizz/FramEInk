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
3. Attendi la finestra **Settings** oppure tocca la notifica **Accedi alla rete**:
   FramEInk si presenta come captive portal. Se non compare, apri **Safari/Chrome**
   su `http://192.168.4.1/` (o all'indirizzo mostrato).
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

Il captive portal funziona su entrambi i modelli, senza Internet. Il DNS locale
risponde alle richieste IPv4 con l'indirizzo della cornice; le verifiche HTTP di
Apple, Android, Windows e altri sistemi vengono reindirizzate alla pagina Settings.
L'apertura della finestra o della notifica dipende dal sistema operativo e dalle
impostazioni della rete: l'accesso diretto tramite IP rimane disponibile. Il DNS
è attivo solo durante i Settings e risponde con TTL zero, così i client non
conservano in cache gli indirizzi locali quando cambiano rete.
Le richieste di riconoscimento non prolungano da sole il timeout del portale.
Se il servizio DNS non riesce ad avviarsi, i Settings rimangono accessibili via IP;
il risultato è indicato nel log `Captive portal DNS=...`.

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
forzato un nuovo tentativo. Il limite complessivo è **120 secondi**: se la connessione fallisce, il firmware
salta le richieste online per quel ciclo, mostra il calendario con i dati
mancanti indicati dai segnaposto meteo e va in sleep. Non riavvia la scheda.
I tentativi riprendono al prossimo ciclo calendario (normalmente dopo due
risvegli, perché l'alternanza con le foto prosegue).

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

## Riavvii imprevisti e log sulla SD

I log sono **attivi automaticamente** su Inkplate 10 e Inkplate 6 Color,
con la SD già usata per le foto. Non richiedono formattazione o configurazione.
Vengono salvati in `/frameink-logs/current.log`; raggiunti 256 KiB, il file
ruota in `/frameink-logs/previous.log`, sostituendo il precedente. Occupazione
massima complessiva: circa **512 KiB**. Le foto restano nelle loro cartelle.
Ogni messaggio è sincronizzato e il file viene chiuso subito; non si tiene
un file di log aperto durante la decodifica delle foto.

Per recuperare i log dopo un problema:

1. Spegni e riaccendi la cornice per aprire i Settings, oppure usa il portale
   già aperto dal recupero dopo reset ripetuti.
2. Collegati alla rete **FramEInk** con la password mostrata sul display e apri
   `http://192.168.4.1/`.
3. Apri **Diagnostics and restart logs**, premi **Download current log** e,
   se disponibile, **Download previous log**. Il precedente esiste solo dopo
   la prima rotazione. La lettura non cancella i log e non li carica in RAM
   tutti insieme. Durante un upload foto termina prima il trasferimento.
4. Conserva entrambi i file per l'analisi. In alternativa, a cornice spenta,
   puoi leggerli dalla SD con il computer.

Il normale deep sleep spegne il Wi-Fi: il portale è accessibile nei Settings,
non durante lo sleep. Spegnere e riaccendere **non cancella** i file di log.
Il firmware registra già al primo riavvio anomalo la causa disponibile, quindi
la successiva accensione manuale non nasconde quel record.

Ogni riga include epoch UTC, millisecondi dall'avvio, numero di avvio della
sessione, memoria libera/minima, blocco massimo allocabile, PSRAM libera e
margine minimo dello stack. Prima del ripristino RTC/NTP l'epoch può non essere
valido: per ricostruire la sequenza usare anche `boot` e `ms`.
Le fasi includono `wifi_connect`, `weather_download`, `calendar_download`,
`calendar_parse`, `photo_decode`, `dashboard_render`, `display_refresh` e
`deep_sleep`. Sono inclusi esiti HTTP, byte ricevuti, numero eventi, errori di
allocazione, disconnessioni Wi-Fi e prossimo risveglio RTC/timer.
La riga `RTC sync` registra anche l'epoch scritto e riletto, la differenza e
l'esito di `isSet()`, per distinguere un orologio non impostato da un'ora errata.
Non vengono registrati SSID, password, URL privati, nomi dei calendari o titoli
degli eventi. Il monitor seriale a **115200 baud** è disponibile su entrambi i modelli.

Il record `boot` distingue `POWERON`, `DEEP_SLEEP`, `SOFTWARE`, `PANIC`,
`TASK_WATCHDOG`, `INT_WATCHDOG`, `WATCHDOG` e `BROWNOUT` tramite
[`esp_reset_reason()` di Espressif](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-reference/system/system.html).
`previous_stage` usa una piccola struttura RTC non reinizializzata al reset,
con firma e checksum; dopo perdita di alimentazione o memoria RTC non valida
può essere `unavailable`. Il contatore di avvii riparte dall'accensione.

Un reset anomalo riprende il calendario senza riaprire i Settings solo se il
contesto RTC è valido e conferma che i Settings erano già stati completati nella
sessione, con credenziali Wi-Fi disponibili. Le sole credenziali salvate non
bastano: un watchdog del bootloader, prima di eseguire il firmware, deve aprire
il portale. Lo stesso vale se il reset interrompe la prima schermata Settings.
I campi `rtc_context` e `resume` del log indicano queste decisioni.
Dopo **tre reset anomali consecutivi**, prima di completare
un ciclo, viene aperto il portale per leggere i log e correggere le impostazioni.
Alla chiusura o scadenza dei 10 minuti di inattività, quel ciclo va in sleep
prima di ritentare al risveglio successivo. La schermata Settings rimane visibile
sull'e-paper durante questa attesa, anche se il Wi-Fi è spento. Un'accensione
manuale continua ad aprire normalmente il portale.

Se il monitor mostra `flash read err, 1000` subito dopo `POWERON_RESET`, seguito
da `RTCWDT_RTC_RESET`, il primo errore avviene nella ROM, prima del codice
FramEInk. Non è un errore del parser del calendario e non può essere scritto
direttamente nei log SD dal firmware. La configurazione VS Code usa **DIO,
40 MHz**, i valori predefiniti del pacchetto Inkplate, da applicare con un nuovo
upload. Questo riduce la frequenza della flash rispetto alla precedente
configurazione a 80 MHz; non dimostra né garantisce la risoluzione dell'errore
di avvio, che va verificata con accensioni reali.

Protezioni del percorso foto → calendario:

- Il download riserva sempre il byte finale `\0`: nessuna ricerca prima
  dell'inizio del buffer e nessuna scrittura oltre la capacità. Il calendario
  ha un limite di 1.999.999 byte di contenuto e viene accettato solo completo;
  una risposta HTML, tronca o con NUL interno viene scartata.
- Le richieste hanno timeout di connessione/lettura di 10 secondi e handshake
  TLS di 15 secondi. La lettura del corpo ha limite totale di 45 secondi e
  inattività massima di 8 secondi. Si richiede HTTP/1.0; risposte che impongono
  comunque chunked o compressione vengono scartate e segnalate nel log.
  I redirect sono limitati a tre. Il calendario fa una richiesta per ciclo,
  senza ripetere indefinitamente gli errori HTTP.
- Il meteo ha un limite di 32 KiB, verifica il JSON e gestisce campi mancanti,
  stringhe lunghe e valori numerici fuori intervallo senza copiare puntatori
  nulli o oltrepassare i campi del modello.
- Il parser del calendario sposta i buffer più grandi dallo stack all'heap,
  verifica le allocazioni, limita il lavoro a 100.000 passi/10 secondi e lascia
  periodicamente eseguire il task idle. Le ricorrenze e i calcoli di data
  sono limitati; le date accettate sono 1970–2037 per il core con `time_t`
  a 32 bit. Gli eventi di molti anni vengono ritagliati ai giorni visibili.
- Le foto illeggibili vengono saltate con al massimo otto tentativi per ciclo.

Questi sono log diagnostici applicativi, **non un core dump o un backtrace**
completo. Non si scrive sulla SD dentro il gestore di crash: l'ultima fase viene
salvata prima dell'operazione e il motivo del reset al successivo avvio.
Un'interruzione di alimentazione durante una scrittura può comunque perdere
l'ultima riga o danneggiare il filesystem. Con SD assente, piena o guasta il
firmware continua con i log seriali; una scrittura fallita disabilita altri
tentativi di log su SD fino al risveglio successivo. Errori prima dell'inizializzazione
SD o dentro i driver richiedono eventualmente anche il monitor seriale.

Test dei nuovi casi di errore, con AddressSanitizer/UndefinedBehaviorSanitizer,
SD/rete/reset simulati e il vero parser ArduinoJson installato per il firmware:

```sh
bash tests/reliability/run.sh
# Su un'altra installazione:
ARDUINOJSON_DIR=/percorso/ArduinoJson/src bash tests/reliability/run.sh
```

Sono coperti limiti dei buffer, frammentazione, download troncati, timeout e
wrap di `millis()`, meteo incompleto/malformato, ricorrenze costose, errori HTTP,
PSRAM indisponibile, alternanza offline, recupero dopo reset, rotazione e guasti
SD. Il test `photo_upload_test.cpp` copre anche i download dei log e il rifiuto
di percorsi arbitrari. Restano da verificare sulle board reali i cicli prolungati,
la SD fisica e l'alimentazione; i test host non riproducono guasti elettrici.

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

Test del captive portal con DNS/UDP/server simulati: query A/AAAA ed EDNS,
nomi lunghi, pacchetti troncati/malformati, redirect delle verifiche dei sistemi
operativi, accesso diretto, timeout e chiusura di DNS/HTTP/AP:

```sh
clang++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -Itests/settings/mocks -Itests/photo_upload/mocks -IFramEink.Core/src/WiFiAccessPoint tests/settings/captive_portal_test.cpp FramEink.Core/src/WiFiAccessPoint/WiFiAPSettings.cpp FramEink.Core/src/WiFiAccessPoint/PhotoUploadService.cpp -o /tmp/frameink-captive-portal-test
/tmp/frameink-captive-portal-test
```

I test host non verificano la comparsa della finestra del sistema operativo:
va provata collegando un telefono o computer alla rete della scheda aggiornata.

## Blocco upload sulla scheda sbagliata

**Upload in VS Code** usa il wrapper incluso in
`.vscode/arduino-cli-wrapper/bin/arduino-cli`. Dopo la prima configurazione o un
cambio di `arduino.path` / `arduino.commandPath`, esegui **Developer: Reload Window**:
l'estensione memorizza il percorso del CLI fino alla ricarica della finestra.
Subito prima della scrittura il wrapper controlla che il firmware corrisponda al modello selezionato e che il MAC
letto dalla scheda collegata sia registrato per quel modello.
Una scheda sconosciuta, il modello sbagliato, un errore di lettura, un registro
non valido o un binario senza identificazione **bloccano l'upload**.
La compilazione con i precedenti `#error` continua a verificare anche
l'abbinamento tra sketch e scheda selezionata.

Le due Inkplate usano entrambe ESP32: l'identificazione del chip effettuata
da esptool non basta a distinguere il display. Occorre associare **una volta**
ciascuna scheda fisica al proprio modello, controllando il modello sulla scheda.
Il wrapper usa il MAC, non il nome della porta USB o il firmware già presente.
Per registrare le due schede, collegale **una alla volta** ed esegui dalla
cartella del progetto, sostituendo la porta se diversa:

```sh
# Eseguire con la vera Inkplate 10 collegata:
python3 tools/board_guard.py register --model Inkplate10 --port /dev/tty.usbserial-110

# Poi scollegarla e collegare la vera Inkplate 6 Color:
python3 tools/board_guard.py register --model Inkplate6COLOR --port /dev/tty.usbserial-110
```

La registrazione legge il MAC con esptool, riavvia la scheda e salva
l'associazione in `.vscode/board-identities.local.json` sul computer.
Non carica firmware e non modifica eFuse o partizioni. Il file è escluso da
Git; una registrazione esistente non viene riassegnata automaticamente a un
modello diverso. Se il file manca, la protezione resta attiva e blocca le schede
non registrate. La dichiarazione iniziale del modello deve essere corretta:
il MAC identifica la singola scheda, non contiene il modello del display.

Il comando `prebuild` di `.vscode/arduino.json` verifica anche la scheda **prima
della compilazione** quando premi Upload: un dispositivo sconosciuto o diverso
interrompe l'operazione subito, anche se l'estensione conserva un vecchio percorso
del CLI. Durante Verify e l'analisi IntelliSense non apre la porta seriale.
Il controllo finale del wrapper ripete la verifica prima della scrittura.

Dopo la registrazione usa normalmente **Upload** in VS Code. Per esempio,
se selezioni 6 Color e colleghi la 10, l'output mostra:

```text
Scheda collegata: Inkplate10 (...). Firmware selezionato: Inkplate6COLOR.
UPLOAD BLOCCATO: collega la scheda corretta.
```

Il controllo legge anche un identificatore incluso nel file `.ino.bin`, prima
di aprire la porta, per evitare di caricare un vecchio binario o quello dell'altro
modello. I binari precedenti a questa protezione devono essere ricompilati.
Il flusso `compile --upload` viene eseguito come compilazione seguita da upload
con verifica finale; se la compilazione fallisce, l'upload non parte.
I messaggi `[Board guard]` sono visibili anche con il livello di log normale
dell'estensione: il completamento viene annunciato soltanto dopo il successo
del comando di scrittura. La sola riga `[Done]` dell'estensione, senza i messaggi
del controllo, non conferma che il percorso protetto sia stato eseguito.
Non occorre installare pyserial: viene usato esptool della piattaforma Arduino.
La lettura MAC ha timeout di 35 secondi; una porta occupata o assente blocca
l'operazione. Chiudi prima il monitor seriale.

La protezione è attiva attraverso il wrapper del progetto. Per usare il
terminale richiama lo stesso wrapper, per esempio:

```sh
.vscode/arduino-cli-wrapper/bin/arduino-cli compile --upload \
  --fqbn Inkplate_Boards:esp32:Inkplate6COLOR:PartitionScheme=huge_app \
  --port /dev/tty.usbserial-110 --build-path .build/inkplate6color FramEink_6Color
```

Il comando globale `arduino-cli`, Arduino IDE e un esptool lanciato direttamente
seguono le proprie procedure: per mantenere il controllo usa VS Code o il
wrapper. Programmatore esterno e OTA non sono supportati da questo percorso.
`.vscode/settings.json` punta al wrapper nella cartella attuale; se sposti il
progetto, aggiorna `arduino.path`. Il vero CLI viene trovato nell'installazione
Arduino IDE su macOS o nel PATH; in alternativa imposta
`FRAMEINK_ARDUINO_CLI=/percorso/del/vero/arduino-cli`.

Test della protezione, senza schede e senza scrivere firmware:

```sh
python3 tests/upload_guard/upload_guard_test.py
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
