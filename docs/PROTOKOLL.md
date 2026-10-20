# Pumpenprotokoll 2

Beide Firmwares müssen zusammen aktualisiert werden. Startbefehle aus dem alten Fünf-Byte-Protokoll werden nicht unterstützt.

## Rahmen

Fest 47 Bytes, Little Endian, 115200 Baud, 8N1.

| Bytes | Inhalt |
| --- | --- |
| 0–1 | `A5 5A` |
| 2 | Version 2 |
| 3 | Nachrichtentyp, Werte in `shared/pump_protocol.h` |
| 4–7 | Auftragskennung, 0 für noch keinen Auftrag |
| 8–11 | Zufällige ESP32-Bootsitzung, ungleich 0 |
| 12–43 | Acht uint32-Laufzeiten in ms; nur bei Start benutzt |
| 44 | Rückmeldung: tatsächliche Ausgabemaske; bei Error: Fehlercode |
| 45–46 | CRC-16/CCITT, Startwert FFFF, Polynom 1021, Bytes 2–44 |

Der Parser verwirft ungültige Versionen, Typen, CRCs und Teilrahmen nach 100 ms Pause. Nach beschädigten Rahmen erfolgt die Wiedererkennung am nächsten vollständigen Header. START wird niemals automatisch erneut gesendet.

## Ablauf

- ESP32 sendet Hello mit neuer Bootsitzung. Eine neue Sitzung stoppt alle Ausgänge, setzt die Arduino-Auftragskennung zurück und beantwortet Ready. Wiederholtes Hello in derselben Sitzung löscht keine bereits verwendeten Auftragskennungen.
- Ein Start enthält alle acht Laufzeiten als einen geprüften Auftrag. 0 bedeutet, diesen Ausgang nicht einzuschalten. 1–300000 ms sind zulässig; mindestens ein Ausgang muss laufen. Erst nach vollständiger Validierung wird die Ausgabemaske geändert.
- Ein bereits laufender anderer Auftrag wird abgelehnt. Ein wiederholter aktueller Auftrag wird bestätigt, verändert aber keine Laufzeit. Ein abgeschlossener oder abgebrochener Auftrag wird nicht erneut ausgeführt. Kleinere/alte Kennungen werden abgelehnt.
- Jeder Kanal wird nach seiner eigenen Laufzeit ausgeschaltet. Status meldet eine geänderte Maske, Done bestätigt die abgeschlossene Ausgabe.
- Der ESP32 sendet alle 250 ms KeepAlive. Nur die aktuelle Sitzung und Auftragskennung verlängern den Arduino-Watchdog. Nach 1500 ms ohne gültiges KeepAlive schaltet der Arduino sämtliche Ausgänge aus.
- Zusätzlich erwartet der ESP32 alle weniger als 1500 ms eine gültige Bedienbestätigung der Sitzung, die den Auftrag gestartet hat. Statusabfragen anderer Browser verlängern diese Frist nicht. Bei fehlender Bedienbestätigung wird Stop ausgelöst.
- Stop schaltet alle acht Ausgänge aus, unabhängig von Rezept, aktiven Pumpenplätzen oder Sitzung. Stopped meldet die tatsächliche Nullmaske. Der ESP32 hält die Bestellsperre bis zu dieser Bestätigung aufrecht und wiederholt erforderlichenfalls Stop. Ohne kompatible Rückmeldung bleibt er gesperrt.
- Ein erkannter Arduino-Neustart während einer Ausgabe führt zum Fehler und Stopp. Weder dieser Neustart noch eine neue Browserverbindung startet den letzten Auftrag erneut.

Die Stoppfunktion ist eine Softwarefunktion über UART und schaltet die Pumpenausgänge. Sie ersetzt keinen unabhängigen Hardware-Not-Aus. Spannungspegel, Treiber, Pegelwandler und Ausgangsverhalten bei Reset werden erst am tatsächlichen Aufbau geprüft.
