/*******************************************************************************
  MPLAB Harmony Application Source File
  
  Company:
    Microchip Technology Inc.
  
  File Name:
    app.c

  Summary:
    This file contains the source code for the MPLAB Harmony application.

  Description:
    This file contains the source code for the MPLAB Harmony application.  It 
    implements the logic of the application's state machine and it may call 
    API routines of other MPLAB Harmony modules in the system, such as drivers,
    system services, and middleware.  However, it does not call any of the
    system interfaces (such as the "Initialize" and "Tasks" functions) of any of
    the modules in the system or make any assumptions about when those functions
    are called.  That is the responsibility of the configuration-specific system
    files.
 *******************************************************************************/

// DOM-IGNORE-BEGIN
/*******************************************************************************
Copyright (c) 2013-2014 released Microchip Technology Inc.  All rights reserved.

Microchip licenses to you the right to use, modify, copy and distribute
Software only when embedded on a Microchip microcontroller or digital signal
controller that is integrated into your product or third party product
(pursuant to the sublicense terms in the accompanying license agreement).

You should refer to the license agreement accompanying this Software for
additional information regarding your rights and obligations.

SOFTWARE AND DOCUMENTATION ARE PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF
MERCHANTABILITY, TITLE, NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE.
IN NO EVENT SHALL MICROCHIP OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER
CONTRACT, NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR
OTHER LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE OR
CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT OF
SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
(INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.
 *******************************************************************************/
// DOM-IGNORE-END


// *****************************************************************************
// *****************************************************************************
// Section: Included Files 
// *****************************************************************************
// *****************************************************************************

#include "app.h"
#include  "GestPWM.h"
#include  "Mc32DriverLcd.h"
#include  "Mc32Delays.h"
#include "Mc32gest_RS232.h"
#include "GestPWM.h"
#include <stdint.h>

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************
/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    This structure should be initialized by the APP_Initialize function.
    
    Application strings and buffers are be defined outside this structure.
*/

APP_DATA appData;

S_pwmSettings PwmData;
S_pwmSettings PwmDataToSend;
// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Functions
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************
/* Fonction :
    void callback_timer1(void)

  Résumé :
    Fonction de rappel du timer 1.

  Description :
    Cette fonction est appelée à chaque déclenchement du timer 1, qui est
    configuré pour des déclenchements tous les 100 ms. Elle maintient un
    compteur statique, et lorsqu'il atteint la valeur 30 (soit après 3 secondes),
    elle déclenche un changement d'état de l'application vers APP_STATE_SERVICE_TASKS
    en appelant la fonction APP_UpdateState elle reappelle cette tout les 100ms 

  Remarques :
    - La fonction est probablement associée à un timer configuré pour des
      déclenchements périodiques tous les 100 ms.

*/
// *****************************************************************************
void callback_timer1(void)
{
        // Déclencher un changement d'état vers APP_STATE_SERVICE_TASKS
        APP_UpdateState(APP_STATE_SERVICE_TASKS);
}

// *****************************************************************************
// *****************************************************************************
// Section: Application Local Functions
// *****************************************************************************
// *****************************************************************************


// *****************************************************************************
/* Fonction :
    void APP_UpdateState(APP_STATES NewState)

  Résumé :
    Met à jour l'état global de l'application.

  Description :
    Cette fonction met à jour l'état global du switch avec la nouvelle
    valeur spécifiée en tant que paramètre. Elle doit être appelée pour changer
    l'état de l'application selon les besoins de la logique de la machine à états.

  Remarques :
    La variable globale appData.state est mise à jour directement avec la nouvelle
    valeur spécifiée en paramètre. Aucune valeur de retour n'est fournie, car la
    modification est effectuée directement sur la variable globale d'état.

    Cette fonction est utilisée pour faciliter la gestion de l'état de l'application
    dans la machine à états principale de l'application.
*/
// *****************************************************************************
void APP_UpdateState(APP_STATES NewState)
{
    // Met à jour l'état de l'application avec la nouvelle valeur
    appData.state = NewState;
    
    // Aucune sortie explicite, car la mise à jour est effectuée directement sur la variable d'état globale.
    // La fonction n'a pas de valeur de retour (void).
}


// *****************************************************************************

// *****************************************************************************
/* Fonction :
    void Allume_Leds(uint8_t ChoixLed)

  Résumé :
    Contrôle l'état des LEDs en fonction du masque de choix spécifié.

  Description :
    Cette fonction prend en entrée un masque de bits (ChoixLed) où chaque bit
    correspond à l'état (allumé ou éteint) d'une LED spécifique. Les bits à 1
    indiquent d'allumer la LED correspondante, tandis que les bits à 0 indiquent
    de l'éteindre. La fonction utilise ce masque pour contrôler l'état de chaque
    LED individuellement.

  Remarques :
    Les LEDs sont identifiées de BSP_LED_0 à BSP_LED_7.
    Le masque ChoixLed permet de sélectionner quelles LEDs doivent être allumées.
    Les LEDs non sélectionnées sont éteintes.

    Exemple :
    - Pour allumer BSP_LED_0 et BSP_LED_3, ChoixLed = 0x09 (00001001 en binaire).
    - Pour éteindre toutes les LEDs, ChoixLed = 0x00.
    - Pour allumer toutes les LEDs, ChoixLed = 0xFF.
*/
// *****************************************************************************
void EteindreLEDS(void)
{
    BSP_LEDOff(BSP_LED_0);
    BSP_LEDOff(BSP_LED_1);
    BSP_LEDOff(BSP_LED_2);
    BSP_LEDOff(BSP_LED_3);
    BSP_LEDOff(BSP_LED_4);
    BSP_LEDOff(BSP_LED_5);
    BSP_LEDOff(BSP_LED_6);
    BSP_LEDOff(BSP_LED_7);
}

void AllumerLEDS(void)
{
    LED0_W = 0;
    LED1_W = 0;
    LED2_W = 0;
    LED3_W = 0;
    LED4_W = 0;
    LED5_W = 0;
    LED6_W = 0;
    LED7_W = 0;
}



// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void APP_Initialize ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Initialize ( void )
{
    /* Place the App state machine in its initial state. */
    appData.state = APP_STATE_INIT;
    
    
    /* TODO: Initialize your application's state machine and other
     * parameters.
     */
}

// Fonction :
// void APP_Tasks(void)

// Résumé :
// Fonction principale de gestion des tâches de l'application.

// Description :
// La fonction APP_Tasks gère les différentes tâches de l'application en fonction
// de l'état actuel. Elle comprend l'initialisation, la réception de paramètres,
// la lecture du potentiomètre, l'affichage, l'exécution PWM, la gestion du moteur,
// l'envoi de valeurs et la mise à jour de l'état.

void APP_Tasks(void)
{
    uint8_t CommStatus;
    static uint8_t IcycleTx;

    /* Vérifier l'état actuel de l'application. */
    switch (appData.state)
    {
        /* État initial de l'application. */
        case APP_STATE_INIT:
        {
            /* Initialisation de l'affichage LCD */
            lcd_init(); 
            printf_lcd("Local settings");
            lcd_gotoxy(1,2);
            printf_lcd("TP2 PWM 2023-2024");
            lcd_gotoxy(1,3);
            printf_lcd("Joao Marques"); 
            lcd_gotoxy(1,4);
            printf_lcd("Damien Bignens"); 
            lcd_bl_on();
            
            /* Initialisations des périphériques */
            GPWM_Initialize(&PwmData);
            
            // Initialisation de l'ADC
            BSP_InitADC10();
            
            // Initialisation de UART (USART_ID_1)
            DRV_USART0_Initialize();
            
            // Initialisation de la communication sérielle           
            InitFifoComm();
                    
            // Mettre à jour l'état 
            APP_UpdateState(APP_STATE_WAIT);
            
            /* Toutes les LED allumées */
            EteindreLEDS();
            
            break;
        }

        case APP_STATE_SERVICE_TASKS:
        {
            // Réception des paramètres distants
            CommStatus = GetMessage(&PwmData);
            
            // Lecture de la position du potentiomètre
            if (CommStatus == 0) // Local?
            {
                GPWM_GetSettings(&PwmData); // Local
            }
            else 
            {
                GPWM_GetSettings(&PwmDataToSend); // Remote
            }
            
            // Affichage des paramètres
            GPWM_DispSettings(&PwmData, CommStatus);
            
            // Exécution de la PWM et gestion du moteur
            GPWM_ExecPWM(&PwmData);
            
            // Effectue l'envoi après 5 cycles
            if (IcycleTx >= 5)
            {
                // Envoi des valeurs
                if (CommStatus == 0) // Local?
                {
                    // SendMessage(&PwmData); // Local
                }
                else 
                {
                    SendMessage(&PwmDataToSend); // Remote
                }
            }
            
            IcycleTx++; // Incrémentation du compteur Cycle

            // Mettre à jour l'état 
            APP_UpdateState(APP_STATE_WAIT);
            
            break;
        }

        case APP_STATE_WAIT:
        {
            // État d'attente, aucune action particulière ici
            break;
        }

        /* L'état par défaut ne devrait jamais être exécuté. */
        default:
        {
            break;
        }
    }
}


/*******************************************************************************
 End of File
 */
