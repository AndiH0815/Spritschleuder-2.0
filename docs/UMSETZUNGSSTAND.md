# Stand 20. Oktober 2026

Erste Firmware-Erweiterung auf Basis von `68d6eafb3ad59853c75596843d3263a1e18437cb`. Erstellt ohne Zugang zur Maschine in Nettetal. Dieser Stand wurde nicht auf Hardware übertragen und noch nicht in Dieters Repository veröffentlicht.

## Abnahme dieses Entwicklungsschritts

- [x] Acht feste Pumpenplätze, persistentes Aktiv-Häkchen und Kalibrierwerte
- [x] Stabile Zutatenkennungen, 400-ml-Basis, Stärke und manuelle Auffüllmenge
- [x] Parallele Dosierung, unabhängiger Stopp und 32-Bit-Laufzeiten
- [x] Doppelte und überlappende Aufträge auf Steuerungsebene abweisen
- [x] Tatsächlichen Abschluss und Stopp bestätigen, nicht aus Countdown ableiten
- [x] Unterbrechung der Bedien-/UART-Verbindung und Controller-Neustart behandeln
- [x] Heim-WLAN ohne Hotspot und ohne begrenzte Zahl von Wiederverbindungsversuchen
- [x] Allgemeiner Login, Verwaltungsfreigabe, HTTPS und serverseitige API-Prüfung
- [x] Alte Rezepte erhalten, markieren und vor Neuinterpretation prüfen
- [x] Hosttests für Kernlogik, beide Steuerungsteile, Speicherung und Sitzungen
- [x] Arduino-Uno-Referenzbuild mit AVR Core 1.8.8 / avr-gcc 7.3.0
- [x] ESP32-C3-Build mit ESP-IDF 6.0.2; rund 940 KiB in bestehender 1-MiB-Anwendungspartition
- [x] Browserprüfung mit Mock-API: Login, Verwaltungsfreigabe, acht Plätze, Kalibrierung, Bestellsperre, Verbindung und bestätigter Stopp; 1024/736/360/320 Pixel ohne horizontales Überlaufen

## Ergänzung: drei Bedienbereiche

- Gastansicht mit Fotokarten, explizitem Start, zwei gespeicherten Glasgrößen und Stärke.
- Bestellsperre und Fortschritt richten sich nach der echten Steuerungsantwort; Wartungsläufe erzeugen keine Meldung über einen fertigen Cocktail.
- Barkeeper verwaltet benannte Zutaten, acht Rezeptzeilen und Glasprofile. Rezeptmengen beziehen sich auf 400 ml fertigen Drink.
- Fotos lassen sich auswählen, zuschneiden und als kleines JPEG lokal speichern. Der Bildbereich liegt zusätzlich hinter der bestehenden App-Partition; Flashgröße und freie Bereiche vor Ort prüfen.
- Maintenance bietet acht feste Plätze mit Aktivierung, Zuordnung, Kalibrierung sowie begrenzten Läufen zum Vorfüllen und Spülen.
- Beide Verwaltungsansichten benötigen die zusätzliche, zeitlich begrenzte Freigabe. Gäste erhalten keine Pumpeneinstellungen.
- Automatische GitHub-Prüfungen für ESP32-C3, reale Speicher-/Steuerungsklassen und Browserabläufe ergänzt. Der aktuelle Prüflauf ist in PR #1 sichtbar.

## Für die vollständige geplante Version noch nötig

- Vereinbarten Waagensensor und Pins festlegen, Sensoradapter und Prüfung vor dem Aktivieren ergänzen; derzeit ist die Überwachung aus.
- Konfiguration exportieren/importieren; Passwortänderung und vollständigen Sitzungswiderruf ergänzen.
- Bestehende WLAN-/Zugangsdaten über einen Wartungsweg erhalten und ändern können. Der USB-Helfer erzeugt bislang ein erstes NVS-Abbild, kein verlustfreies Update eines bestehenden Geräts.
- Mit Dieter die tatsächlichen Boards, Firmwareversionen, Ausgangspegel, Stromversorgung, Pins und verfügbaren Speicher prüfen.
- Flash/NVS sichern, Zertifikat auf dem alten Tablet prüfen und beide Firmwares gemeinsam testen: zunächst ohne Pumpenlast, dann mit Wasser und Messgefäß, schließlich in finaler Schlauchführung.

## Prüfanleitung vor realer Ausgabe

1. Nach Reset bleiben alle Ausgänge aus. Unpassende oder alte Firmware startet keine Pumpe.
2. Deaktivierte Plätze können weder Rezept, Vorfüllen, Spülen noch Kalibrierlauf starten. Ein neuer aktiver Platz lässt sich ohne bisherigen Kalibrierwert begrenzt testen.
3. Ein Auftrag startet genau einmal. Alle benötigten Pumpen laufen parallel; jeder Kanal schaltet unabhängig ab. Eine zweite Bestellung bleibt gesperrt.
4. Stopp erreicht alle acht Kanäle, auch deaktivierte. Ohne Stoppbestätigung keine neue Bestellung.
5. Tabletverbindung und UART getrennt unterbrechen sowie beide Controller getrennt neu starten. Kein automatischer Wiederanlauf.
6. Kalibrierwerte pro Leitung messen und speichern. Alter Vier-Pumpen-Rezeptbestand muss bis zur Prüfung unbestellbar bleiben.
7. Ohne Login jede API direkt aufrufen; ohne Verwaltungsfreigabe Pumpentest/Einstellungen versuchen. Beide Zugriffe müssen scheitern.
8. Internet zum Router trennen, lokales WLAN erhalten: Bedienung bleibt erreichbar. Router ausschalten: Pumpen stoppen und kein Hotspot erscheint.

Dies ist eine Softwareprüfung mit abschließend erforderlichem Hardwareabgleich. Der Bildspeicher und die neue Oberfläche benötigen zusätzlich die Prüfung auf dem tatsächlichen ESP32 und dem alten Android-Tablet. Die Waagenüberwachung bleibt bis zur Auswahl und Implementierung des Sensoradapters ausgeschaltet.
