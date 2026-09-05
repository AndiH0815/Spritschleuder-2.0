# Spritschleuder 2

Cocktailautomat mit ESP32-C3, 128×128-ST7735-Display, Weboberfläche und einem separaten Arduino für vier Pumpen.

## Schnellstart für die Entwicklung

Voraussetzung ist eine eingerichtete ESP-IDF-Umgebung (aktuell getestet mit ESP-IDF 6.0.2).

```sh
. /opt/esp-idf/export.sh
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Beim ersten Build lädt der ESP-IDF Component Manager die offizielle Komponente `espressif/qrcode`. Die erzeugte ESP32-C3-Firmware liegt anschließend unter `build/cocktailbot.bin`.

Der Pumpencontroller ist ein eigenes Ziel: `main/arduino_main.cpp` muss zusätzlich mit der Arduino-IDE oder PlatformIO auf den angeschlossenen Arduino geladen werden. Ohne den aktuellen Sketch funktionieren parallele Pumpenlaufzeiten und Sofortabbruch nicht korrekt.

## Bedienung

- Ohne gespeichertes WLAN startet das Gerät den Access Point `Spritschleuder-Setup` mit Passwort `cocktail`. Die Weboberfläche ist dann unter `http://192.168.4.1` erreichbar.
- K1 (GPIO 8) stoppt alle Pumpen unmittelbar. Nach ungefähr drei Sekunden kehrt das Display selbstständig zur Hauptansicht zurück; es wird nicht auf ein Arduino-ACK gewartet.
- K2 (GPIO 10) schaltet zwischen Haupt- und Infoansicht um. Die Infoansicht zeigt IP-Adresse und QR-Code zur Geräte-URL.
- Neue Mixturen werden in der Weboberfläche als relative ml-Anteile je Pumpe angelegt und dauerhaft im NVS gespeichert. Maximal sind 20 Mixturen vorgesehen.

## Aufbau

- `main/main.cpp`: Initialisierung und Startreihenfolge
- `main/cocktail_server.cpp`: eingebettete Weboberfläche und HTTP-API
- `main/recipes.cpp`: Rezeptverwaltung und NVS-Persistenz
- `main/arduino_comm.cpp`: UART-Protokoll, Befehlswarteschlange und Abbruch
- `main/arduino_main.cpp`: Firmware des separaten Pumpen-Arduinos
- `main/display_manager.cpp`: Displayzustände, Fortschritt und QR-Code
- `main/button_manager.cpp`: entprelltes Polling der beiden Gerätetasten
- `main/config.h`: Pins, WLAN-, UART- und Displaykonfiguration

## UART-Protokoll

Ein Pumpenbefehl besteht aus fünf Bytes:

```text
0xAA  PUMP_ID  DURATION_HI  DURATION_LO  0x55
```

Die Laufzeit ist eine 16-Bit-Zahl in Millisekunden. Mehrere Startbefehle werden direkt hintereinander übertragen; der Arduino verwaltet für jede Pumpe eine eigene Abschaltzeit. Ein Befehl mit Dauer `0` schaltet die betreffende Pumpe sofort aus. Beim Abbruch werden solche AUS-Befehle für alle vier Pumpen gesendet.

## HTTP-API

- `GET /api/status` – WLAN- und Verbindungsstatus
- `GET /api/recipes` – Liste aller Mixturen
- `POST /api/recipes` – Mixtur mit `name` und `pump0_ml` bis `pump3_ml` speichern
- `POST /api/order` – Mixtur mit `cocktail_id` und `quantity_ml` starten
- `POST /api/abort` – laufende Mischung sofort abbrechen
- `POST /api/wifi` und `POST /api/wifi/reset` – WLAN konfigurieren beziehungsweise löschen

## Hinweise für Änderungen

Nach Änderungen immer mindestens `idf.py build` ausführen. Bei Arbeiten an Pumpensteuerung oder UART müssen beide Firmwares aktualisiert und gemeinsam auf echter Hardware geprüft werden. Pinänderungen gehören zentral nach `main/config.h`; das Board-Schaltbild liegt als `ESP32-C3-1.44inch.pdf` im Projektordner.
