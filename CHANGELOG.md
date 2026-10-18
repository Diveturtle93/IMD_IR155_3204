# Changelog

Alle nennenswerten Änderungen an diesem Projekt werden hier dokumentiert.
Format nach [Keep a Changelog](https://keepachangelog.com/de/1.1.0/),
Versionierung nach [Semantic Versioning](https://semver.org/lang/de/).

## [Unveröffentlicht]

## [1.0.0] - 2026-10-17

Erste veröffentlichte Version der Bibliothek.

### Hinzugefügt

- Auswertung des PWM-Signals des Bender IR155-3204: Zuordnung der Frequenz zu
  Statuscodes (0, 10, 20, 30, 40 und 50 Hz) und Berechnung des
  Isolationswiderstands in kOhm aus dem Duty-Cycle.
- Unterscheidung zwischen Kurzschluss gegen Masse und gegen KL15 bei 0 Hz über
  den direkt eingelesenen Pegel am PWM-Pin.
- Plausibilitätsprüfung zwischen IMD-OK-Pin und ausgewertetem PWM-Status.
- `imd_init()`: Initialisierung von Timer 1 im Input-Capture-Modus (Kanal 1
  steigende, Kanal 2 fallende Flanke).
- `imd_process()`: Berechnung von Frequenz und Duty-Cycle aus den
  Capture-Werten und zyklische Statusauswertung alle 5 s.
- `IMD_TIM_CaptureCallback()`: Interrupt-Callback zur Erfassung der
  Capture-Werte mit gleitendem Mittelwert.
- Felder `ImdOK` und `IMD_PWM` in `imd_tag`.
- Versionsmakros `IMD_MAJOR`, `IMD_MINOR`, `IMD_PATCH` und `IMD_DEV` in `imd.h`.
- Optionale Debug-Ausgabe über UART (`DEBUG_IMD`).

### Geändert

- Abhängigkeit vom Batteriemanagement-Projekt (`BatteriemanagementSystem.h`,
  `sdc_in`, `system_out`, `system_in`) entfernt. Der IMD-Status wird direkt
  über die HAL-GPIO-Funktionen gelesen und ausgegeben.
- Bitpositionen von `Resistanc` und `PWM_STATUS` in `imd_tag` korrigiert.
- Versionsangabe aus den Dateiheadern entfernt; die Version wird nur noch über
  die Makros in `imd.h` und die Git-Tags geführt.
- Code- und Kommentarformatierung vereinheitlicht.

[Unveröffentlicht]: https://github.com/Diveturtle93/IMD_IR155_3204/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/Diveturtle93/IMD_IR155_3204/releases/tag/v1.0.0
