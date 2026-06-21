//----------------------------------------------------------------------
// Titel	:	imd.c
//----------------------------------------------------------------------
// Sprache	:	C
// Datum	:	29.08.2021
// Version	:	1.0
// Autor	:	Diveturtle93
// Projekt	:	IMD
//----------------------------------------------------------------------

// Einfuegen der standard Include-Dateien
//----------------------------------------------------------------------
#include <math.h>
//----------------------------------------------------------------------

// Einfuegen der STM Include-Dateien
//----------------------------------------------------------------------
#include "main.h"
#include "tim.h"
//----------------------------------------------------------------------

// Einfuegen der eigenen Include Dateien
//----------------------------------------------------------------------
#include "imd.h"
#include "my_math.h"
#include "millis.h"
//----------------------------------------------------------------------

// Variablen einbinden
//----------------------------------------------------------------------
imd_tag imd;																// Variable fuer IMD Eigenschaften definieren

static volatile uint16_t rising = 0, falling = 0;							// Timer Capture Werte
static volatile uint8_t pwm_change = 0;										// Flag: PWM Pegel hat sich geaendert
static uint32_t timer1Periode = 0;											// Timer1 Periode fuer Frequenzberechnung
//----------------------------------------------------------------------

// IMD Timer initialisieren
//----------------------------------------------------------------------
void imd_init (void)
{
	if (HAL_TIM_Base_Init(&htim1) != HAL_OK);
	if (HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_1) != HAL_OK);
	if (HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_2) != HAL_OK);
	timer1Periode = (HAL_RCC_GetPCLK2Freq() / (htim1.Init.Prescaler / 2));
}
//----------------------------------------------------------------------

// IMD PWM Auswertung
//----------------------------------------------------------------------
void imd_process (void)
{
	static uint32_t timeIMD = 0;

	// Wenn sich der Pegel am IMD_PWM aendert
	if (pwm_change == 1)
	{
		// IMD Werte berechnen
		if (rising != 0 && falling != 0)
		{
			// Berechne DutyCycle und Frequenz von IMD_PWM
			uint16_t imd_diff = getDifference(rising, falling);
			imd.DutyCycle = 100 - round((float)(imd_diff * 100) / (float)rising);
			imd.Frequency = timer1Periode / rising;
		}

		// Alle 5s den IMD-Status auslesen
		if (millis() > (timeIMD + 5000))
		{
			imd_status();
			timeIMD = millis();
		}

		pwm_change = 0;
	}
}
//----------------------------------------------------------------------

// IMD Status einlesen
//----------------------------------------------------------------------
void imd_status (void)
{
	// Einlesen von IMD Ok Pin
	sdc_in.IMD_OK_IN = HAL_GPIO_ReadPin(IMD_OK_IN_GPIO_Port, IMD_OK_IN_Pin);		// IMD OK einlesen

	// Abfrage ob IMD Ok ist
	if (sdc_in.IMD_OK_IN != 1)
	{
		// Ausgabe IMD OK kommend BMS
		imd.ImdOK = 1;
		HAL_GPIO_WritePin(IMD_OK_OUT_GPIO_Port, IMD_OK_OUT_Pin, imd.ImdOK);			// IMD Status von BMS ausgeben
	}
	else
	{
		// Ausgabe IMD nicht Ok kommend BMS
		imd.ImdOK = 0;
		HAL_GPIO_WritePin(IMD_OK_OUT_GPIO_Port, IMD_OK_OUT_Pin, imd.ImdOK);			// IMD Status von BMS ausgeben
	}

#ifdef DEBUG_IMD
	// Ausgabe Frequenz
	uartTransmit("Frequenz: \t", 11);
	uartTransmitNumber(imd.Frequency, 10);
	uartTransmit("\n", 1);

	// Ausgabe DutyCycle
	uartTransmit("DutyCycle: \t", 12);
	uartTransmitNumber(imd.DutyCycle, 10);
	uartTransmit("\n", 1);
#endif

	// IMD PWM abfragen
	switch (imd.Frequency)
	{
		case 1:
		case 0:																		// Case 0 Hz
			// PWM Pin einlesen
			imd.IMD_PWM = HAL_GPIO_ReadPin(IMD_PWM_GPIO_Port, IMD_PWM_Pin);

			// IMD-Widerstand auf Null setzen
			imd.Resistanc = 0;

			// Wenn IMD 1 ist
			if (imd.IMD_PWM == 1)
			{
				// IMD Status speichern
				imd.PWM_STATUS = IMD_KURZSCHLUSS_KL15;								// Kurzschluss von HV nach Pluspol
			}
			// Wenn IMD 0 ist
			else
			{
				// IMD Status speichern
				imd.PWM_STATUS = IMD_KURZSCHLUSS_GND;								// Kurzschluss von HV nach Masse
			}
			break;

		case 9:
		case 11:
		case 10:																	// Case 10 Hz
			// IMD Status speichern
			imd.PWM_STATUS = IMD_NORMAL;											// IMD funktioniert normal

			// DutyCycle abfragen
			if (imd.DutyCycle > 5 && imd.DutyCycle <= 95)							// IMD PWM
			{
				// Widerstand berechnen
				imd.Resistanc = 90 * 1200 / (imd.DutyCycle - 5) - 1200;				// Angabe in kOhm

#ifdef DEBUG_IMD
				// Ausgabe Widerstandswert
				uartTransmit("Widerstand: \t", 13);
				uartTransmitNumber(imd.Resistanc, 10);
				uartTransmit("\n", 1);
#endif
			}
			// Falls DutyCycle nicht im Wertebereich ist
			else																	// IMD Invalid
			{
				// IMD Status speichern
				imd.PWM_STATUS = IMD_DUTY_ERROR;									// Fehlerausgabe
			}
			break;

		case 19:
		case 21:
		case 20:																	// Case 20 Hz
			// IMD Status speichern
			imd.PWM_STATUS = IMD_UNTERSPANNUNG;										// Unterspannung an HV erkannt

			// DutyCycle abfragen
			if (imd.DutyCycle > 5 && imd.DutyCycle <= 95)							// IMD PWM
			{
				// Widerstand berechnen
				imd.Resistanc = 90 * 1200 / (imd.DutyCycle - 5) - 1200;				// Angabe in kOhm

#ifdef DEBUG_IMD
				// Ausgabe Widerstandswert
				uartTransmit("Widerstand: \t", 13);
				uartTransmitNumber(imd.Resistanc, 10);
				uartTransmit("\n", 1);
#endif
			}
			// Falls DutyCycle nicht im Wertebereich ist
			else																	// IMD Invalid
			{
				// IMD Status speichern
				imd.PWM_STATUS = IMD_DUTY_ERROR;									// Fehlerausgabe
			}
			break;

		case 29:
		case 31:
		case 30:																	// Case 30 Hz
			// IMD Status speichern
			imd.PWM_STATUS = IMD_SCHNELLSTART;										// Schnellstartmessung

			// DutyCycle abfragen
			if (imd.DutyCycle >= 5 && imd.DutyCycle < 11)							// IMD Gut
			{

			}
			else if (imd.DutyCycle > 89 && imd.DutyCycle <= 95)						// IMD Schlecht
			{

			}
			// Falls DutyCycle nicht im Wertebereich ist
			else																	// IMD Invalid
			{
				// IMD Status speichern
				imd.PWM_STATUS = IMD_DUTY_ERROR;									// Fehlerausgabe
			}
			break;

		case 39:
		case 41:
		case 40:																	// Case 40 Hz
			// IMD Status speichern
			imd.PWM_STATUS = IMD_GERAETEFEHLER;										// Geraetefehler

			// DutyCyle abfragen
			if (imd.DutyCycle > 47 && imd.DutyCycle < 53)							// IMD PWM
			{

			}
			// Falls DutyCycle nicht im Wertebereich ist
			else																	// IMD Invalid
			{
				// IMD Status speichern
				imd.PWM_STATUS = IMD_DUTY_ERROR;									// Fehlerausgabe
			}
			break;

		case 49:
		case 51:
		case 50:																	// Case 50 Hz
			// IMD Status speichern
			imd.PWM_STATUS = IMD_ANSCHLUSSFEHLER_ERDE;								// Anschluss an Erde festgestellt

			// DutyCycle abfragen
			if (imd.DutyCycle > 47 && imd.DutyCycle < 53)							// IMD PWM
			{

			}
			// Fall DutyCycle nicht im Wertebereich ist
			else																	// IMD Invalid
			{
				// IMD Status speichern
				imd.PWM_STATUS = IMD_DUTY_ERROR;									// Fehlerausgabe
			}
			break;

		default:																	// Case Default Fehler
			// IMD Status speichern
			imd.PWM_STATUS = IMD_FREQ_ERROR;										// Fehlerausgabe
			break;
	}

	// Abfrage Plausibilitaet am IMD
	if ((sdc_in.IMD_OK_IN == 1) && (imd.PWM_STATUS != 10))
	{
		imd.PWM_STATUS = IMD_PLAUS_ERROR;											// Plausibilitaetsfehler bei IMD ok und falschem Status
	}

#ifdef DEBUG_IMD
	// Ausgabe Status
	uartTransmit("Status: \t", 9);
	uartTransmitNumber(imd.PWM_STATUS, 10);
	uartTransmit("\n", 1);
#endif
}
//----------------------------------------------------------------------

// Timer Interrupt fuer IMD PWM-Auswertung
//----------------------------------------------------------------------
void IMD_TIM_CaptureCallback (TIM_HandleTypeDef *htim)
{
	// Timer 1 fuer IMD PWM-Auswertung
	if (htim == &htim1)
	{
		pwm_change = 1;
		if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
		{
			rising = calculateMovingAverage(rising, HAL_TIM_ReadCapturedValue(&htim1, TIM_CHANNEL_1), 10);
		}
		else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2)
		{
			falling = calculateMovingAverage(falling, HAL_TIM_ReadCapturedValue(&htim1, TIM_CHANNEL_2), 10);
		}
	}
}
//----------------------------------------------------------------------