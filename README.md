# IMD IR155-3204
 
Eine in C implementierte Treiberbibliothek für STM32-Mikrocontroller zur Ansteuerung und
Auswertung des Isolationswächters **Bender IR155-3204** (Insulation Monitoring Device, IMD).
Der IMD überwacht den Isolationswiderstand zwischen dem Hochvoltsystem und der
Fahrzeugkarosserie und meldet seinen Status über ein PWM-Signal.
 
## Beschreibung
 
Der IR155-3204 codiert seinen Status über die Frequenz eines PWM-Signals und den
gemessenen Isolationswiderstand über das Tastverhältnis (Duty-Cycle) desselben Signals.
Die Bibliothek wertet beide Größen aus, ordnet die Frequenz einem definierten Statuscode
zu und berechnet bei gültigem Status den aktuellen Isolationswiderstand in kΩ.
 
Zusätzlich zur PWM-Auswertung wird der digitale **IMD-OK-Pin** eingelesen. Dieser dient als
schnelle Plausibilitätsprüfung: Ist der IMD-OK-Pin gesetzt, der über die PWM ausgewertete
Status aber nicht „Normal" (10 Hz), wird ein Plausibilitätsfehler erkannt. Der ausgewertete
IMD-Status wird über einen separaten Ausgangspin an das übergeordnete System weitergegeben.
 
## Dateien
 
| Datei   | Beschreibung                                                              |
|---------|-----------------------------------------------------------------------------|
| `imd.h` | Statuscodes, `imd_tag`-Union für PWM-Auswertung, Funktionsdeklaration        |
| `imd.c` | Implementierung der PWM-Frequenzauswertung und Widerstandsberechnung        |
 
## PWM-Statuscodierung
 
Der IR155-3204 signalisiert seinen Zustand über die Frequenz des PWM-Signals:
 
| Frequenz | Status                  | Konstante                    | Bedeutung                                          |
|:--------:|:-------------------------|:------------------------------|:-----------------------------------------------------|
| 0 Hz     | Kurzschluss gegen Masse  | `IMD_KURZSCHLUSS_GND`        | HV-Kurzschluss gegen Fahrzeugmasse                  |
| 10 Hz    | Normalzustand            | `IMD_NORMAL`                 | IMD arbeitet normal, Widerstand wird berechnet      |
| 20 Hz    | Unterspannung            | `IMD_UNTERSPANNUNG`          | Unterspannung am HV-System erkannt                  |
| 30 Hz    | Schnellstart-Messung     | `IMD_SCHNELLSTART`           | IMD führt eine Schnellstart-Messung durch            |
| 40 Hz    | Gerätefehler             | `IMD_GERAETEFEHLER`          | Interner Fehler des IMD                              |
| 50 Hz    | Anschlussfehler Erde     | `IMD_ANSCHLUSSFEHLER_ERDE`   | Fehlerhafter Anschluss gegen Erde festgestellt       |
| 0 Hz     | Kurzschluss gegen KL15   | `IMD_KURZSCHLUSS_KL15`       | HV-Kurzschluss gegen Pluspol (KL15)                  |
| –        | Frequenzfehler           | `IMD_FREQ_ERROR`             | Gemessene Frequenz außerhalb des gültigen Bereichs   |
| –        | Duty-Cycle-Fehler        | `IMD_DUTY_ERROR`             | Tastverhältnis außerhalb des für die Frequenz gültigen Bereichs |
| –        | Plausibilitätsfehler     | `IMD_PLAUS_ERROR`            | IMD-OK-Pin gesetzt, aber Status ungleich „Normal"   |
 
Da 0 Hz sowohl für „Kurzschluss gegen Masse" als auch für „Kurzschluss gegen KL15" steht,
wird in diesem Fall zusätzlich der digitale PWM-Pegel direkt eingelesen, um zwischen
beiden Fehlerfällen zu unterscheiden.
 
## Widerstandsberechnung
 
Im Normalzustand (10 Hz, sowie 20 Hz bei Unterspannung) wird der Isolationswiderstand aus
dem Duty-Cycle berechnet, sofern dieser im gültigen Bereich von 5–95 % liegt:
 
```c
Widerstand [kOhm] = 90 * 1200 / (DutyCycle - 5) - 1200
```
 
Liegt der Duty-Cycle außerhalb dieses Bereichs, wird stattdessen `IMD_DUTY_ERROR` gesetzt.
 
## Datenstruktur `imd_tag`
 
Die Bibliothek nutzt eine gepackte Union, um die ausgewerteten PWM-Eigenschaften
effizient zu speichern:
 
| Feld         | Bits  | Beschreibung                                |
|--------------|:-----:|----------------------------------------------|
| `Frequency`  | 6     | Gemessene PWM-Frequenz                       |
| `Resistanc`  | 18    | Berechneter Isolationswiderstand in kΩ       |
| `PWM_STATUS` | 4     | Statuscode (siehe Tabelle oben)              |
| `DutyCycle`  | 7     | Gemessenes Tastverhältnis in Prozent         |
 
```c
extern imd_tag imd;
```
 
`Frequency` und `DutyCycle` müssen von der aufrufenden Applikation (z. B. über Timer-Input-
Capture) ermittelt und in die globale Variable `imd` geschrieben werden, bevor
`imd_status()` aufgerufen wird.
 
## API
 
```c
void imd_status (void);    // IMD-Status auswerten und Widerstand berechnen
```
 
`imd_status()` liest zusätzlich den `IMD_OK_IN`-Pin ein, aktualisiert den `IMD_OK_OUT`-Pin
entsprechend und führt die Plausibilitätsprüfung zwischen Pin-Status und PWM-Status durch.
 
## Verwendung
 
### 1. Dateien einbinden
 
`imd.h` und `imd.c` in das STM32-Projekt kopieren und den Header einbinden:
 
```c
#include "imd.h"
```
 
### 2. Frequenz und Duty-Cycle ermitteln
 
Die PWM-Frequenz und der Duty-Cycle des IR155-3204 müssen über eine Timer-Input-Capture-
Messung (z. B. mit zwei Capture-Kanälen für steigende und fallende Flanke) im Projekt
ermittelt und in die globale Variable geschrieben werden:
 
```c
imd.Frequency = gemessene_frequenz;
imd.DutyCycle = gemessener_dutycycle;
```
 
### 3. Status auswerten
 
```c
imd_status();
 
// Status und Widerstand auswerten
if (imd.PWM_STATUS == IMD_NORMAL)
{
    // imd.Resistanc enthaelt den aktuellen Isolationswiderstand in kOhm
}
else
{
    // Fehlerfall ueber imd.PWM_STATUS behandeln
}
```
 
### 4. Debug-Ausgabe aktivieren (optional)
 
In `imd.h` kann die Debug-Ausgabe über UART aktiviert werden, indem das Makro
`DEBUG_IMD` einkommentiert wird (nur wirksam im Debug-Build, wenn `DEBUG` definiert ist):
 
```c
#ifdef DEBUG
#define DEBUG_IMD
#endif
```
 
Im aktivierten Zustand gibt `imd_status()` Frequenz, Duty-Cycle, berechneten Widerstand
und den finalen Status über UART aus.
 
## Hinweise
 
- Die Bibliothek erwartet projektspezifische Pin-Definitionen für `IMD_OK_IN`,
  `IMD_OK_OUT` und `IMD_PWM` sowie die Strukturen `sdc_in` und `system_out` aus dem
  übergeordneten Projekt.
- Diese Bibliothek wird im [EAuto_BMS](https://github.com/Diveturtle93/EAuto_BMS) zur
  Überwachung der HV-Isolation eingesetzt.
- Ein Isolationsfehler ist sicherheitsrelevant: Im EAuto_BMS führt ein dauerhaft
  signalisierter Fehlerstatus zur Abschaltung des Hochvoltsystems.
  
## Abhängigkeiten
 
- `main.h` – STM32 HAL
- `BatteriemanagementSystem.h` – projektspezifischer Header (Pin-Definitionen, Strukturen `sdc_in`, `system_out`, `system_in`)
- `basicuart.h` – UART-Sende- und Empfangsfunktionen (nur bei aktivierter Debug-Ausgabe)

## Lizenz
 
Dieses Projekt steht unter der [GPL-3.0 Lizenz](LICENSE).
