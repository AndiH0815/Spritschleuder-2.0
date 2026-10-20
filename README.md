# Spritschleuder 2 – Entwicklungsstand für acht Pumpen

ESP32-C3 mit ST7735-Display, passwortgeschützter lokaler Browserbedienung über das Heim-WLAN und separatem Pumpen-Arduino. Grundlage ist Commit `68d6eafb3ad59853c75596843d3263a1e18437cb`.

Die eingebettete Browseroberfläche verbindet Gast, Barkeeper und Maintenance mit der Maschinensteuerung. Die Software wurde noch nicht auf die Maschine übertragen. Der vorhandene Hardware-Aufbau muss vor der Freigabe geprüft werden.

## Funktionen dieses Stands

- **Gast:** Fotokarten, zwei gespeicherte Glasprofile, leicht/normal/stark, ausdrücklicher Start, Fortschritt und genaue Auffüllanweisung nach bestätigtem Abschluss.
- **Barkeeper:** Rezepteditor mit acht Zutatenzeilen, Stärkemarkierung je Zutat, benannte Zutaten mit stabilen Kennungen und zwei veränderbare Glasprofile. Zutatenbezeichnungen und Glasprofile werden auf dem ESP32 gespeichert.
- **Maintenance:** acht Aktiv-Häkchen, Zuordnung nach Namen, Kalibrierung, einzelne zeitbegrenzte Läufe zum Vorfüllen/Spülen und Verbindungsstatus. Die zusätzliche Verwaltungsfreigabe schützt Barkeeper und Maintenance; dies sind zwei Ansichten derselben Verwaltungsberechtigung.
- **Fotos:** Auswahl und Ausschnitt im Browser; Umwandlung auf 640 × 400 Pixel/JPEG bis 48 KiB. Die Datei liegt lokal in einer eigenen SPIFFS-Partition. Ein fehlgeschlagener Ersatz erhält den bisherigen Bildverweis. Der Webserver streamt die Datei und benötigt keinen großen Bildpuffer. Keine externen Bilddienste oder Internetverbindung zur Anzeige.

- Acht feste Pumpenplätze, dauerhaft gespeicherte Aktivierung, Zutatenkennung und Kalibrierung in ml/s. Ausgeschaltete Plätze behalten ihre Daten und lassen sich weder zum Dosieren noch für Tests starten.
- Rezepte verwenden stabile Zutatenkennungen statt Pumpennummern. Bis zu acht Zutaten, 400-ml-Grundrezept, Glasmenge, leicht/normal/stark und separate manuelle Auffüllmenge. Kohlensäurehaltige Füllzutaten bleiben außerhalb der Pumpen.
- Parallele Pumpenlaufzeiten bis fünf Minuten mit unabhängigem Abschalten; keine stille Begrenzung auf 65.535 ms.
- Zentraler Auftragszustand, Ablehnung paralleler Bestellungen und bereits verwendeter Start-Sequenzen. Kein automatisches Wiederholen von Startbefehlen nach einem Verbindungsfehler.
- Neues UART-Protokoll mit CRC, Sitzung, Auftragskennung, Annahme-, Status-, Fertig- und Stoppbestätigung. Ein abgelaufener Countdown bedeutet noch keinen Abschluss.
- Verbindungsüberwachung auf ESP32 und Arduino. Verlust des aktiven Bedientablets, Kommunikationsfehler und Neustarts führen zum Stopp. Ein fremder Browser hält einen Auftrag nicht am Leben.
- Heim-WLAN im Station-Modus, wiederholte Verbindungsversuche, kein eigener Hotspot – auch bei fehlenden Zugangsdaten.
- HTTPS ohne HTTP-Rückfall, Bedienpasswort, zeitlich begrenzte Sitzung und zusätzliche Verwaltungsfreigabe. Serverseitige Prüfung aller geschützten Funktionen.
- USB-Ersteinrichtung über ein lokal erzeugtes privates NVS-Abbild; keine Zugangsdaten im Quellcode. Ohne Zugangsdaten und Zertifikat bleibt der Webserver geschlossen.
- Übernahme alter Vier-Pumpen-Rezepte mit Prüfmarkierung. Alte Mengen und der alte Speicherblock bleiben erhalten. Keine automatische Umdeutung in neue 400-ml-Rezepte und kein automatisches Löschen von NVS.

## Bewusst noch offen

- Anpassung an die tatsächlich verbaute Hardware, Pumpenpegel, Versorgung, Kalibrierung und reale Auslauftests.
- Waagenmodell, Pins, Hardwareadapter und Sensorgrenzwerte. Die Wiegeüberwachung ist ausgeschaltet; dieser Stand allein erfüllt das spätere Nachrüsten ohne Firmwarewechsel noch nicht. Dafür muss der vereinbarte Sensoradapter vor der endgültigen Inbetriebnahme ergänzt werden.
- Passwortwechsel, Widerruf aller Sitzungen, Datenexport/-import und WLAN-Wechsel mit getesteter Rückkehr zu den alten Zugangsdaten. Das Provisionierungswerkzeug ist ausschließlich für eine gesicherte **Ersteinrichtung** gedacht.
- Prüfung des lokalen Zertifikats und des alten Android-Browsers. „Angemeldet bleiben“ gilt bis zu 30 Tage, aber ein Neustart der Steuerung verwirft in diesem ersten Stand alle Sitzungen.

## ESP32 bauen

Getestete Basis: ESP-IDF **6.0.2**, Ziel **esp32c3**. Die verwalteten Abhängigkeiten stehen in `dependencies.lock`.

```sh
. /pfad/zu/esp-idf/export.sh
idf.py set-target esp32c3
idf.py build
```

Größenoptimierung ist aktiviert, damit HTTPS in die bestehende 1-MiB-Anwendungspartition passt. NVS (0x9000, 24 KiB), PHY und die 1-MiB-Anwendung ab 0x10000 bleiben an ihren bisherigen Adressen. Die neue Tabelle `partitions.csv` ergänzt den Bildspeicher ab 0x110000 bis 0x400000; sie erfordert mindestens 4 MiB Flash. Die bisherige 16-MB-Flash-Annahme muss vor Ort geprüft und passend eingestellt werden. Ein reines App-Update installiert keine neue Partitionstabelle. Vor jeder Änderung vollständigen Flash sichern und prüfen, dass der zusätzliche Bereich frei ist. Es wird kein bestehendes oder beschädigtes Bilddateisystem automatisch formatiert; nur ein vollständig als gelöscht geprüfter Bildbereich wird erstmals eingerichtet.

`CONFIG_SPRIT_HARDWARE_CONFIRMED` ist standardmäßig **aus**. Dann akzeptiert die Firmware keine Pumpenstarts, auch bei gültiger Anmeldung. Erst nach Hardwareprüfung und Testaufbau darf diese Option in `idf.py menuconfig` unter „Spritschleuder“ gesetzt werden. Alle Pumpenplätze sind anfangs deaktiviert und unkalibriert.

Der separate Arduino benötigt ebenfalls die neue Firmware; das alte Fünf-Byte-Protokoll wird nicht mehr zum Starten verwendet. Die ESP32-Firmware wartet auf eine kompatible Gegenstelle. Ein altes Sketch erhält beim ESP32-Start nur acht AUS-Befehle.

## Arduino bauen

Die reale Boardbezeichnung ist noch unbekannt. Als Übersetzungsprüfung dient ein Arduino Uno/ATmega328P mit Arduino AVR Core 1.8.8 und avr-gcc 7.3.0. Das ist **keine** Bestätigung des eingebauten Boards.

```sh
AVR_TOOLCHAIN=/pfad/zu/avr \
ARDUINO_AVR_CORE=/pfad/zu/ArduinoCore-avr \
./tools/build_arduino.sh
```

Pins 2–9 und HIGH=EIN stammen aus dem alten Sketch und müssen vor dem Aufspielen geprüft werden. `main/arduino_main.cpp` verwendet ausschließlich die AVR-kompatiblen Header in `shared/`; keine zusätzliche STL-Bibliothek ist erforderlich. USB-Seriell auf einem Uno kann dessen Reset auslösen; beide Start-/Resetfälle gehören zum Hardwaretest.

## Zugang und erste Einrichtung

Vor jedem Flashen vorhandenen Flash/NVS sichern, Controller und Pins identifizieren. Hier wurde **keine** Hardware beschrieben.

1. Eine feste lokale Geräte-IP am Router reservieren.
2. In der aktivierten ESP-IDF-Umgebung `python tools/provision.py --ip <lokale-IP>` ausführen. Das Werkzeug fragt WLAN sowie zwei unterschiedliche Passwörter ab und erzeugt ein gerätespezifisches Zertifikat sowie `private-setup/initial-nvs.bin`. Es verweigert das Überschreiben eines vorhandenen Ausgabeverzeichnisses.
3. Das Abbild enthält WLAN und Zugangsdaten für ein neu eingerichtetes Gerät. Es würde beim Schreiben an NVS **alle bisherigen NVS-Einstellungen ersetzen**. Niemals ohne Sicherung über bestehende Rezepte schreiben. Vor Ort ist die Übernahme/Ergänzung der bestehenden NVS-Daten zu klären.
4. Das Zertifikat auf dem vorgesehenen Tablet prüfen und vertrauenswürdig einrichten; `https://<lokale-IP>` öffnen. Bei fehlendem Vertrauen nicht auf unverschlüsseltes HTTP ausweichen.

Passwörter werden als PBKDF2-HMAC-SHA256-Hash mit zufälligem Salt und 100.000 Iterationen abgelegt. Sitzungstokens sind zufällig, HttpOnly, Secure und SameSite=Strict. Die Verwaltungsfreigabe läuft nach 15 Minuten ab. Der lokale Flash ist nicht gegen physischen Zugriff verschlüsselt; private Sicherungen schützen.

## Tests

Nach Änderungen mindestens `idf.py build`; bei UART-Änderungen auch den Arduino bauen und vor Einsatz beide Firmwares zusammen am Gerät prüfen.

```sh
CJSON_SOURCE=managed_components/espressif__cjson/cJSON ./tools/test.sh
```

Hosttests benötigen einen C/C++-Compiler, OpenSSL-Entwicklungsbibliothek und cJSON aus dem ESP-IDF-Build. Sie prüfen Protokoll/CRC, Dosierung, unabhängige Laufzeiten, Wiederholungen, Zählerüberlauf, Watchdogs, bestätigtes Stoppen, verlorene Antworten, Neustarts, Altdatenmigration, Speicherfehler, Sitzungen und Berechtigungsabgrenzung. Der Hostadapter für PSA verwendet OpenSSL; die reale Firmware verwendet ESP-IDFs PSA-Implementierung. Speicher-/Sitzungstests ersetzen keine NVS-/TLS-Prüfung am ESP32.

Die Browserprüfung `tests/test_web.cjs` benötigt Playwright und eine Chromium-Installation (`SPRIT_CHROMIUM`). Sie verwendet eine Mock-API und prüft drei Menüs, Rezeptmengen, Stärkeskalierung, Zutaten, Glasprofile, Bildzuschnitt/-upload, acht Plätze, Kalibrierung, bestätigten Stopp, Wartungsabschlüsse und vier Displaybreiten. GitHub Actions führt Firmwarebuild, Hosttests und Browserprüfung für den Entwicklungs-PR aus. Sie ist keine Prüfung des echten HTTPS-Servers.

## Quellcode

- `shared/pump_protocol.h`: fester Protokollrahmen und Parser
- `shared/pump_board.h`: Arduino-Auftragszustand und Ausgabemaske
- `shared/dosing.h`: Rezept-, Stärke- und Laufzeitberechnung
- `main/arduino_comm.cpp`: ESP32-Auftragszustand, Gegenstellen- und Bedienverbindung
- `main/arduino_main.cpp`: eigenständiger Arduino-Adapter und Pinansteuerung
- `main/access_control.cpp`: Anmeldung, Sitzungen und Verwaltungsfreigabe
- `main/cocktail_server.cpp`: geschützte API; `main/web.html`: Gast-, Barkeeper- und Maintenance-Oberfläche
- `main/recipes.cpp`, `main/pump_settings.cpp`, `main/bar_settings.cpp`: NVS-Speicherung
- `main/photo_store.cpp`: lokaler Bildspeicher und vorsichtige Ersteinrichtung
- `main/wifi_manager.cpp`: Station-Modus ohne Hotspot

Details: [Protokoll](docs/PROTOKOLL.md), [Änderungsstand und Abnahme](docs/UMSETZUNGSSTAND.md). Lizenz: GPL-3.0 wie im ursprünglichen Repository.
