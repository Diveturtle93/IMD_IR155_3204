# IMD IR155-3204

Eine in C implementierte Treiberbibliothek für STM32-Mikrocontroller zur Ansteuerung und
Auswertung des Isolationswächters **Bender IR155-3204** (Insulation Monitoring Device, IMD).
Der IMD überwacht den Isolationswiderstand zwischen dem Hochvoltsystem und der
Fahrzeugkarosserie und meldet seinen Status über ein PWM-Signal.

**Aktuelle Version:** 1.0.0. Alle Änderungen stehen im [CHANGELOG](CHANGELOG.md).

## Beschreibung

Der IR155-3204 codiert seinen Status über die Frequenz eines PWM-Signals und den
gemessenen Isolationswiderstand über das Tastverhältnis (Duty-Cycle) desselben Signals.
Die Bibliothek misst beide Größen per Timer-Input-Capture, ordnet die Frequenz einem
definierten Statuscode zu und berechnet bei gültigem Status den aktuellen
Isolationswiderstand in kΩ.

Zusätzlich zur PWM-Auswertung wird der digitale **IMD-OK-Pin** eingelesen. Dieser dient als
schnelle Plausibilitätsprüfung: Passt der Pegel am IMD-OK-Pin nicht zum über die PWM
ausgewerteten Status, wird ein Plausibilitätsfehler erkannt. Der IMD-Status wird über einen
separaten Ausgangspin an das übergeordnete System weitergegeben.

## Dateien

| Datei   | Beschreibung                                                                  |
|---------|-------------------------------------------------------------------------------|
| `imd.h` | Versionsmakros, Statuscodes, `imd_tag`-Union, Funktionsdeklarationen          |
| `imd.c` | Timer-Initialisierung, PWM-Messung, Statusauswertung und Widerstandsberechnung |

## PWM-Statuscodierung

Der IR155-3204 signalisiert seinen Zustand über die Frequenz des PWM-Signals:

| Frequenz | Status                  | Konstante                    | Bedeutung                                          |
|:--------:|:------------------------|:-----------------------------|:---------------------------------------------------|
| 0 Hz     | Kurzschluss gegen Masse | `IMD_KURZSCHLUSS_GND`        | HV-Kurzschluss gegen Fahrzeugmasse                 |
| 10 Hz    | Normalzustand           | `IMD_NORMAL`                 | IMD arbeitet normal, Widerstand wird berechnet     |
| 20 Hz    | Unterspannung           | `IMD_UNTERSPANNUNG`          | Unterspannung am HV-System erkannt                 |
| 30 Hz    | Schnellstart-Messung    | `IMD_SCHNELLSTART`           | IMD führt eine Schnellstart-Messung durch          |
| 40 Hz    | Gerätefehler            | `IMD_GERAETEFEHLER`          | Interner Fehler des IMD                            |
| 50 Hz    | Anschlussfehler Erde    | `IMD_ANSCHLUSSFEHLER_ERDE`   | Fehlerhafter Anschluss gegen Erde festgestellt     |
| 0 Hz     | Kurzschluss gegen KL15  | `IMD_KURZSCHLUSS_KL15`       | HV-Kurzschluss gegen Pluspol (KL15)                |
| –        | Frequenzfehler          | `IMD_FREQ_ERROR`             | Gemessene Frequenz außerhalb des gültigen Bereichs |
| –        | Duty-Cycle-Fehler       | `IMD_DUTY_ERROR`             | Tastverhältnis außerhalb des für die Frequenz gültigen Bereichs |
| –        | Plausibilitätsfehler    | `IMD_PLAUS_ERROR`            | IMD-OK-Pin passt nicht zum PWM-Status              |

Jede Frequenz wird mit einer Toleranz von ±1 Hz akzeptiert. Da 0 Hz sowohl für
„Kurzschluss gegen Masse“ als auch für „Kurzschluss gegen KL15“ steht, wird in diesem Fall
zusätzlich der Pegel am PWM-Pin direkt eingelesen, um beide Fehlerfälle zu unterscheiden.

## Widerstandsberechnung

Im Normalzustand (10 Hz, sowie 20 Hz bei Unterspannung) wird der Isolationswiderstand aus
dem Duty-Cycle berechnet, sofern dieser im gültigen Bereich von 5–95 % liegt:

```c
Widerstand [kOhm] = 90 * 1200 / (DutyCycle - 5) - 1200
```

Liegt der Duty-Cycle außerhalb dieses Bereichs, wird stattdessen `IMD_DUTY_ERROR` gesetzt.

## Datenstruktur `imd_tag`

Die Bibliothek nutzt eine gepackte Union (5 Byte), um die ausgewerteten Eigenschaften
effizient zu speichern:

| Feld         | Bits  | Beschreibung                                |
|--------------|:-----:|---------------------------------------------|
| `Frequency`  | 6     | Gemessene PWM-Frequenz in Hz                |
| `Resistanc`  | 18    | Berechneter Isolationswiderstand in kΩ      |
| `PWM_STATUS` | 4     | Statuscode (siehe Tabelle oben)             |
| `ImdOK`      | 1     | Ausgegebener IMD-OK-Status                  |
| `IMD_PWM`    | 1     | Eingelesener Pegel am PWM-Pin (nur bei 0 Hz) |
| `DutyCycle`  | 7     | Gemessenes Tastverhältnis in Prozent        |

```c
extern imd_tag imd;
```

## API

```c
void imd_init (void);                                     // Timer 1 im Input-Capture-Modus starten
void imd_process (void);                                  // Frequenz/Duty-Cycle berechnen, Status zyklisch auswerten
void imd_status (void);                                   // IMD-Status auswerten und Widerstand berechnen
void IMD_TIM_CaptureCallback (TIM_HandleTypeDef *htim);   // Capture-Werte im Interrupt erfassen
```

- `imd_init()` startet Timer 1 mit Kanal 1 (steigende Flanke) und Kanal 2 (fallende Flanke)
  im Input-Capture-Interrupt-Modus und bestimmt die Timerfrequenz.
- `IMD_TIM_CaptureCallback()` übernimmt die Capture-Werte beider Kanäle als gleitenden
  Mittelwert und markiert eine Pegeländerung.
- `imd_process()` berechnet nach einer Pegeländerung Frequenz und Duty-Cycle und ruft
  alle 5 s `imd_status()` auf.
- `imd_status()` liest den `IMD_OK_IN`-Pin ein, setzt den `IMD_OK_OUT`-Pin, wertet den
  PWM-Status aus und führt die Plausibilitätsprüfung durch.

## Verwendung

### 1. Dateien einbinden

`imd.h` und `imd.c` in das STM32-Projekt kopieren und den Header einbinden:

```c
#include "imd.h"
```

### 2. Hardware konfigurieren

In CubeMX Timer 1 mit Kanal 1 und 2 als Input Capture (steigende bzw. fallende Flanke) auf
das PWM-Signal des IMD konfigurieren und den Capture-Interrupt aktivieren. Außerdem werden
die GPIOs `IMD_OK_IN`, `IMD_OK_OUT` und `IMD_PWM` benötigt.

### 3. Initialisieren und zyklisch auswerten

```c
// Capture-Callback der HAL an die Bibliothek weiterreichen
void HAL_TIM_IC_CaptureCallback (TIM_HandleTypeDef *htim)
{
    IMD_TIM_CaptureCallback(htim);
}

int main (void)
{
    // ... HAL- und Peripherie-Initialisierung
    imd_init();

    while (1)
    {
        imd_process();

        if (imd.PWM_STATUS == IMD_NORMAL)
        {
            // imd.Resistanc enthaelt den aktuellen Isolationswiderstand in kOhm
        }
        else
        {
            // Fehlerfall ueber imd.PWM_STATUS behandeln
        }
    }
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

## Versionierung

Die Bibliothek folgt [Semantic Versioning](https://semver.org/lang/de/). Die Version ist in
`imd.h` über die Makros `IMD_MAJOR`, `IMD_MINOR` und `IMD_PATCH` hinterlegt und entspricht
dem Git-Tag `vMAJOR.MINOR.PATCH`. `IMD_DEV` ist bei veröffentlichten Versionen `0`.

## Hinweise

- Ein Isolationsfehler ist sicherheitsrelevant und muss in der Applikation zur Abschaltung
  des Hochvoltsystems führen.
- Diese Bibliothek wird im [EAuto_BMS](https://github.com/Diveturtle93/EAuto_BMS) zur
  Überwachung der HV-Isolation eingesetzt.

## Abhängigkeiten

- `main.h` – STM32 HAL und Pin-Definitionen (`IMD_OK_IN`, `IMD_OK_OUT`, `IMD_PWM`)
- `tim.h` – Timer-Handle `htim1`
- `my_math.h` – `getDifference()` und `calculateMovingAverage()`
- `millis.h` – `millis()` für die zyklische Auswertung
- `basicuart.h` – `uartTransmit()` und `uartTransmitNumber()` (nur bei aktivierter Debug-Ausgabe)

## Lizenz

Dieses Projekt steht unter der [GPL-3.0 Lizenz](LICENSE).
