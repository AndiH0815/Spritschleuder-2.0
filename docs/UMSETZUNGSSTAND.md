# Stand 20. Oktober 2026

Erste Firmware-Erweiterung auf Basis von `68d6eafb3ad59853c75596843d3263a1e18437cb`. Erstellt ohne Zugang zur Maschine in Nettetal. Dieser Stand wurde nicht auf Hardware übertragen und noch nicht in Dieters Repository veröffentlicht.

## Abnahme dieses Entwicklungsschritts

- [x] Acht feste Pumpenplätze, persistentes Aktiv-Häkchen und Kalibrierwerte
- [x] Stabile Zutatenkennungen, 400-ml-Basis, Stärke und manuelle Auffüllmenge
- [x] Parallele Dosierung, unabhängiger Stopp und 32-Bit-Laufzeiten
- [x] Doppelte und überlappende Aufträge auf Steuerungsebene abweisen
- [x] Tatsächlichen Abschluss und Stopp bestätigen, nicht aus Countdown ableiten
- [x] Unterbrechung der Bedien-/UART-Verbindung und Controller-Neustart behandeln
- [x] Heim-WLAN mit Wiederverbindung und geschütztem Setup-Hotspot bei Ausfall oder langem Info-Tastendruck
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
- Passwortänderung und verlustfreie Zertifikatswartung ergänzen. WLAN-Wechsel ist über Maintenance umgesetzt; der USB-Helfer erzeugt weiterhin ausschließlich ein erstes NVS-Abbild.
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
8. Internet zum Router trennen, lokales WLAN erhalten: Bedienung bleibt erreichbar. Router ausschalten: Pumpen stoppen; nach 60 Sekunden erscheint nur der geschützte Setup-Hotspot. Falsche neue Daten, Stromausfall, erfolgreichen Wechsel, lange Info-Taste während Ausgabe und verlorene Stoppbestätigung am Gerät prüfen.

Dies ist eine Softwareprüfung mit abschließend erforderlichem Hardwareabgleich. Der Bildspeicher und die neue Oberfläche benötigen zusätzlich die Prüfung auf dem tatsächlichen ESP32 und dem alten Android-Tablet. Die Waagenüberwachung bleibt bis zur Auswahl und Implementierung des Sensoradapters ausgeschaltet.

## Ergänzung: WLAN-Setup und Wiederherstellung

- SSID genau `spritschleuder`, WPA2-Passwort wie vom Eigentümer festgelegt `12345678`; HTTPS und bestehende Berechtigungen bleiben erforderlich.
- 60 Sekunden WLAN-Ausfall, fünf Sekunden Info-Taste oder Maintenance starten das Setup. Kein Internettest. Netzwechsel nach bestätigtem Pumpenstopp, zentrale Startsperre für alle Ausgaben.
- Neue WLAN-Daten erst testen, dann als ein Profil speichern. Fehlversuch behält bisheriges WLAN und Setup-Zugang. Übergangszeit vor automatischem AP-Abschalten: 15 Sekunden.
- Hosttests prüfen die echte WifiManager-Klasse mit simuliertem Treiber/NVS: Timer, WPA2-Konfiguration, Profile/Altdaten, Zeitüberschreitung, Speicherfehler, Neustart, offene Netze, manuellen/automatischen Modus und Warten auf Stopp. Die echte Steuerung prüft zusätzlich die zentrale Startsperre und verlorene Stopprückmeldung. Entprellung, kurzer/langer Druck und Zählerüberlauf sind geprüft.
- Browserprüfung ergänzt um geschütztes WLAN-Menü, Passwortleerung, Testfehler, Link zur neuen IP, Startsperre und schmale Ansichten. Reale Funk-, DHCP-, TLS-, Flash-/Stromausfall- und Tabletprüfung folgt in Nettetal. Das initiale Zertifikat muss für die Setup-IP gültig und auf dem Tablet vertrauenswürdig sein; der USB-Generator berücksichtigt dies jetzt.

## Ergänzung: feste lokale Geräteadresse

- `https://spritschleuder.local` per offizieller mDNS-Komponente auf STA und AP, HTTPS-Dienst erst nach gestartetem Server; DHCP-Hostname `spritschleuder`.
- Der Zugriff ist mit verschiedenen Browsergeräten vorgesehen, jeweils mit Anmeldung. Eine zusätzliche Bestellung bleibt während der Ausgabe zentral gesperrt. Das ursprüngliche feste Tablet ist keine Gerätebindung.
- IP-Ersatzweg und Setup-IP bleiben erhalten. Menüverknüpfung und Info-QR verwenden bei aktivem Namensdienst den lokalen Namen.
- Neue Ersteinrichtungszertifikate berücksichtigen Gerätenamen und beide IP-Adressen. Bestehende Zertifikate/NVS werden durch diesen Code weder umgeschrieben noch gelöscht.
- Hosttests prüfen Diensttyp/Port, fehlende vertrauliche TXT-Daten, Lebenszyklus und Fehlerbereinigung; OpenSSL prüft die tatsächlich erzeugten Zertifikate auf DNS-Namen, beide IP-Adressen, Deduplizierung und passenden Schlüssel. Browserprüfung prüft Namenslink und IP-Rückfall. Reale Namensauflösung auf iPhone/iPad, PC und vorhandenem Android-Tablet sowie nach Netzwechsel folgt am Gerät.
