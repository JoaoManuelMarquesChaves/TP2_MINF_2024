// Mc32Gest_RS232.C
// Canevas manipulatio TP2 RS232 SLO2 2017-18
// Fonctions d'émission et de réception des message
// CHR 20.12.2016 ajout traitement int error
// CHR 22.12.2016 evolution des marquers observation int Usart
// SCA 03.01.2018 nettoyé réponse interrupt pour ne laisser que les 3 ifs

#include <xc.h>
#include <sys/attribs.h>
#include "system_definitions.h"
// Ajout CHR
#include <GenericTypeDefs.h>
#include "app.h"
#include "GesFifoTh32.h"
#include "Mc32gest_RS232.h"
#include "gestPWM.h"
#include "Mc32CalCrc16.h"
#include <stdlib.h>
#include <stdint.h>
typedef union {
        uint16_t val;
        struct {uint8_t lsb;
                uint8_t msb;} shl;
} U_manip16;


// Definition pour les messages
#define MESS_SIZE  5
// avec int8_t besoin -86 au lieu de 0xAA
#define STX_code  0xAA //(-86)

// Structure décrivant le message
typedef struct {
    uint8_t Start;
    uint8_t  Speed;
    uint8_t  Angle;
    uint8_t MsbCrc;
    uint8_t LsbCrc;
} StruMess;


// Struct pour émission des messages
StruMess TxMess;
// Struct pour réception des messages
StruMess RxMess;

// Declaration des FIFO pour réception et émission
#define FIFO_RX_SIZE ( (4*MESS_SIZE) + 1)  // 4 messages
#define FIFO_TX_SIZE ( (4*MESS_SIZE) + 1)  // 4 messages

int8_t fifoRX[FIFO_RX_SIZE];
// Declaration du descripteur du FIFO de réception
S_fifo descrFifoRX;


int8_t fifoTX[FIFO_TX_SIZE];
// Declaration du descripteur du FIFO d'émission
S_fifo descrFifoTX;


// Initialisation de la communication sérielle
void InitFifoComm(void)
{    
    // Initialisation du fifo de réception
    InitFifo ( &descrFifoRX, FIFO_RX_SIZE, fifoRX, 0 );
    // Initialisation du fifo d'émission
    InitFifo ( &descrFifoTX, FIFO_TX_SIZE, fifoTX, 0 );
    
    // Init RTS 
    RS232_RTS = 1;   // interdit émission par l'autre
   
} // InitComm

 
// *****************************************************************************
/* Fonction :
    int GetMessage(S_pwmSettings *pData)

  Résumé :
    Cette fonction gère la réception de messages depuis une FIFO série.
    Elle récupère les données de la FIFO série, vérifie l'intégrité du message
    à l'aide d'un calcul de CRC, et met à jour la structure de paramètres PWM
    (pData) en conséquence si le message est valide.

  Description :
    La fonction commence par vérifier le nombre de caractères disponibles
    dans la FIFO série. Si le nombre de caractères est suffisant et que le
    premier octet (Start byte) du message est correct, la fonction extrait
    les champs du message (Speed et Angle), calcul le CRC, et vérifie
    l'intégrité du message. Si le CRC est correct, la fonction met à jour
    les paramètres de vitesse et d'angle dans la structure pData, et retourne
    un statut indiquant qu'un message a été reçu avec succès.

    Si le message est incorrect (CRC incorrect ou Start byte incorrect),
    la fonction clignote une LED pour signaler une erreur, incrémente un
    compteur de cycles de réception, et ne met pas à jour la structure pData.
    Si aucun message valide n'est reçu pendant plusieurs cycles, la fonction
    réinitialise le statut de communication.

    La gestion du contrôle de flux de réception est également effectuée
    pour autoriser ou désautoriser l'émission par l'autre partie.

  Paramètres :
    - pData : Un pointeur vers la structure de paramètres PWM (S_pwmSettings)
              contenant les informations de vitesse et d'angle.

  Valeur de retour :
    - 0 : Aucun message valide reçu, les données locales (pData) restent inchangées.
    - 1 : Un message valide a été reçu et les données locales (pData) ont été mises à jour.

*/
int GetMessage(S_pwmSettings *pData) {
    static uint8_t commStatus = 0;  // Statut de la communication (0 = pas de message, 1 = message reçu)
    uint8_t NbCharToRed = 0;  // Nombre de caractères à lire dans la FIFO
    static int IcycleRx = 0;  // Compteur de cycles de réception
    uint16_t ValCrc;  // Valeur du CRC

    // Vérifier le nombre de caractères à lire dans la FIFO série
    NbCharToRed = GetReadSize(&descrFifoRX);

    // Lire le premier octet de la FIFO (Start byte)
    GetCharFromFifo(&descrFifoRX, &RxMess.Start);

    // Si le nombre de caractères est suffisant et le Start byte est correct (0xAA)
    if ((NbCharToRed >= MESS_SIZE) & (RxMess.Start == STX_code)) {
        ValCrc = 0xFFFF;  // Initialiser la valeur du CRC

        // Mettre à jour le CRC avec le Start byte
        ValCrc = updateCRC16(ValCrc, RxMess.Start);

        // Lire le champ Speed et mettre à jour le CRC
        GetCharFromFifo(&descrFifoRX, &RxMess.Speed);
        ValCrc = updateCRC16(ValCrc, RxMess.Speed);

        // Lire le champ Angle et mettre à jour le CRC
        GetCharFromFifo(&descrFifoRX, &RxMess.Angle);
        ValCrc = updateCRC16(ValCrc, RxMess.Angle);

        // Lire les octets du CRC (MSB et LSB) et mettre à jour le CRC
        GetCharFromFifo(&descrFifoRX, &RxMess.MsbCrc);
        ValCrc = updateCRC16(ValCrc, RxMess.MsbCrc);
        GetCharFromFifo(&descrFifoRX, &RxMess.LsbCrc);
        ValCrc = updateCRC16(ValCrc, RxMess.LsbCrc);

        ValCrc = ValCrc;  // Ignorer la valeur finale du CRC (cette ligne semble inutile)

        // Vérifier si le CRC est correct
        if (ValCrc == 0) {
            // Mettre à jour les valeurs de vitesse et d'angle dans la structure pData
            pData->SpeedSetting = RxMess.Speed;
            pData->AngleSetting = RxMess.Angle;

            // Mettre à jour le statut de communication
            commStatus = 1;
            IcycleRx = 0;
        } else {
            // Clignoter une LED pour signaler une erreur CRC
            LED6_W = !LED6_R;
            
            // Réinitialiser le statut de communication et incrémenter le compteur de cycles
            commStatus = 0;
            IcycleRx++;
        }
    } else {
        // Si le nombre de caractères à lire est insuffisant, ou si le Start byte est incorrect,
        // gérer le compteur de cycles de réception
        if (IcycleRx >= 10) {
            commStatus = 0;
            IcycleRx = 0;
        } else {
            IcycleRx++;
        }
    }
    // Gestion du contrôle de flux de la réception
    if (GetWriteSpace(&descrFifoRX) >= (2 * MESS_SIZE)) {
        // Autoriser l'émission par l'autre (abaisser le signal RTS)
        RS232_RTS = 0;
    }
    return commStatus;  // Retourner le statut de communication
} // GetMessage


// *****************************************************************************
/* Fonction :
    void SendMessage(S_pwmSettings *pData)

  Résumé :
    Cette fonction gère l'envoi cyclique de messages via une FIFO série.
    Elle compose un message avec les paramètres de vitesse et d'angle contenus
    dans la structure pData, calcule le CRC, et dépose le message dans la FIFO
    d'émission. Elle gère également le contrôle de flux en autorisant ou
    désautorisant l'émission en fonction de l'état du signal CTS (Clear To Send).

  Description :
    La fonction commence par vérifier si suffisamment d'espace est disponible
    dans la FIFO série pour écrire un message complet. Si c'est le cas, elle
    compose le message en utilisant le format défini, y compris le Start byte,
    les champs Speed et Angle, ainsi que les octets du CRC. Elle utilise la
    fonction updateCRC16 pour calculer le CRC.

    Ensuite, la fonction dépose chaque octet du message dans la FIFO d'émission
    en utilisant la fonction PutCharInFifo.

    Enfin, la fonction gère le contrôle de flux. Si le signal CTS est bas (0)
    et qu'il y a au moins un caractère à envoyer dans la FIFO d'émission, elle
    autorise l'interruption d'émission pour déclencher l'envoi du message.

  Paramètres :
    - pData : Un pointeur vers la structure de paramètres PWM (S_pwmSettings)
              contenant les informations de vitesse et d'angle.

*/
void SendMessage(S_pwmSettings *pData)
{
    int8_t freeSize;
    U_manip16 valCrc16;
    valCrc16.val = 0xFFFF;

    // Test si place pour écrire 1 message
    freeSize = GetWriteSpace(&descrFifoTX);

    if (freeSize >= MESS_SIZE)
    {
        // Compose le message
        TxMess.Start = 0xAA;
        TxMess.Speed = pData->SpeedSetting;
        TxMess.Angle = pData->AngleSetting;

        // Calcule le CRC
        valCrc16.val = updateCRC16(valCrc16.val, TxMess.Start);
        valCrc16.val = updateCRC16(valCrc16.val, TxMess.Speed);
        valCrc16.val = updateCRC16(valCrc16.val, TxMess.Angle);
        TxMess.MsbCrc = valCrc16.shl.msb;
        TxMess.LsbCrc = valCrc16.shl.lsb;

        // Dépose le message dans la FIFO d'émission
        PutCharInFifo(&descrFifoTX, TxMess.Start);
        PutCharInFifo(&descrFifoTX, TxMess.Speed);
        PutCharInFifo(&descrFifoTX, TxMess.Angle);
        PutCharInFifo(&descrFifoTX, TxMess.MsbCrc);
        PutCharInFifo(&descrFifoTX, TxMess.LsbCrc);
    }

    // Gestion du contrôle de flux
    freeSize = GetReadSize(&descrFifoTX);

    if ((RS232_CTS == 0) && (freeSize > 0))
    {
        // Autorise l'interruption d'émission
        PLIB_INT_SourceEnable(INT_ID_0, INT_SOURCE_USART_1_TRANSMIT);
    }
}



// *****************************************************************************
/* Routine d'interruption USART1

  Résumé :
    Cette routine gère les interruptions liées à l'USART1, incluant les erreurs,
    la réception (RX), et la transmission (TX). Elle effectue le traitement
    approprié en fonction de la nature de l'interruption.

  Description :
    La routine commence par marquer le début de l'interruption avec l'allumage
    de la LED3.

    Ensuite, elle vérifie si l'interruption est liée à une erreur. Si c'est le cas,
    elle traite l'erreur en lisant et en jetant les caractères du buffer de réception.

    En cas d'interruption de réception (RX), elle lit les caractères du buffer
    matériel de réception et les dépose dans la FIFO de réception (descrFifoRX). Elle
    active également la LED4 à chaque réception de caractère. Elle gère également
    le contrôle de flux en ajustant la sortie RS232_RTS en fonction de la place
    disponible dans la FIFO de réception.

    Pour l'interruption de transmission (TX), elle envoie les caractères de la FIFO
    de transmission (descrFifoTX) vers le buffer matériel de transmission tant que
    les conditions d'émission sont remplies (CTS = 0, FIFO non vide, buffer d'émission non plein).
    La LED5 est activée à chaque émission de caractère. Elle désactive l'interruption de
    transmission lorsque tous les caractères ont été émis.

    Finalement, la routine marque la fin de l'interruption avec l'extinction de la LED3.

  Note :
    Cette routine assume que la gestion des FIFOs, des LEDs, des signaux de contrôle
    (RS232_RTS et RS232_CTS) ainsi que l'initialisation des périphériques USART1 et des
    interruptions ont été correctement configurées ailleurs dans le code.

*/
void __ISR(_UART_1_VECTOR, ipl5AUTO) _IntHandlerDrvUsartInstance0(void)
{
    USART_ERROR UsartStatus;
    int8_t i_cts;
    uint8_t TXsize, freeSize;
    BOOL TxBuffFull;
    uint8_t caractere;

    // Marque début interruption avec Led3
    LED3_W = 1;

    // Is this an Error interrupt ?
    if (PLIB_INT_SourceFlagGet(INT_ID_0, INT_SOURCE_USART_1_ERROR) &&
        PLIB_INT_SourceIsEnabled(INT_ID_0, INT_SOURCE_USART_1_ERROR))
    {
        /* Clear pending interrupt */
        PLIB_INT_SourceFlagClear(INT_ID_0, INT_SOURCE_USART_1_ERROR);
        // Traitement de l'erreur à la réception.
        while (PLIB_USART_ReceiverDataIsAvailable(USART_ID_1))
        {
            PLIB_USART_ReceiverByteReceive(USART_ID_1);
        }
    }

    // Is this an RX interrupt ?
    if (PLIB_INT_SourceFlagGet(INT_ID_0, INT_SOURCE_USART_1_RECEIVE) &&
        PLIB_INT_SourceIsEnabled(INT_ID_0, INT_SOURCE_USART_1_RECEIVE))
    {

        // Oui Test si erreur parité ou overrun
        UsartStatus = PLIB_USART_ErrorsGet(USART_ID_1);

        if ((UsartStatus & (USART_ERROR_PARITY |
                            USART_ERROR_FRAMING | USART_ERROR_RECEIVER_OVERRUN)) == 0)
        {

            // Lecture des caractères depuis le buffer HW -> fifo SW
            // pour savoir s'il y a une data dans le buffer HW RX
            while (PLIB_USART_ReceiverDataIsAvailable(USART_ID_1))
            {
                // Traitement RX à faire ICI
                // Lecture des caractères depuis le buffer HW -> fifo SW
                c = PLIB_USART_ReceiverByteReceive(USART_ID_1);
                PutCharInFifo(&descrFifoRX, c);
            }

            LED4_W = !LED4_R; // Toggle Led4
            // buffer is empty, clear interrupt flag
            PLIB_INT_SourceFlagClear(INT_ID_0, INT_SOURCE_USART_1_RECEIVE);
        }
        else
        {
            // Suppression des erreurs
            // La lecture des erreurs les efface sauf pour overrun
            if ((UsartStatus & USART_ERROR_RECEIVER_OVERRUN) == USART_ERROR_RECEIVER_OVERRUN)
            {
                PLIB_USART_ReceiverOverrunErrorClear(USART_ID_1);
            }
        }

        // Traitement controle de flux reception à faire ICI
        // Gerer sortie RS232_RTS en fonction de place dispo dans fifo reception
        freeSize = GetWriteSpace(&descrFifoRX);
        if (freeSize <= 6) // a cause d'un int pour 6 char
        {
            // Demande de ne plus émettre
            RS232_RTS = 1;
        }
    } // end if RX

    // Is this a TX interrupt ?
    if (PLIB_INT_SourceFlagGet(INT_ID_0, INT_SOURCE_USART_1_TRANSMIT) &&
        PLIB_INT_SourceIsEnabled(INT_ID_0, INT_SOURCE_USART_1_TRANSMIT))
    {

        // Avant d'émettre, on vérifie 3 conditions :
        //  Si CTS = 0 autorisation d'émettre (entrée RS232_CTS)
        //  S'il y a un caratères à émettre dans le fifo
        //  S'il y a de la place dans le buffer d'émission
        i_cts = RS232_CTS;
        TXsize = GetReadSize(&descrFifoTX);
        TxBuffFull = PLIB_USART_TransmitterBufferIsFull(USART_ID_1);

        if ((i_cts == 0) && (TXsize > 0) && TxBuffFull == false )
        {
            do
            {
                // Traitement TX à faire ICI
                // Envoi des caractères depuis le fifo SW -> buffer HW
                GetCharFromFifo(&descrFifoTX, &c);
                PLIB_USART_TransmitterByteSend(USART_ID_1, c);
                LED5_W = !LED5_R; // Toggle Led5
                i_cts = RS232_CTS;
                TXsize = GetReadSize(&descrFifoTX);
                TxBuffFull = PLIB_USART_TransmitterBufferIsFull(USART_ID_1);
            } while ((i_cts == 0) && (TXsize > 0) && TxBuffFull == false);

            // disable TX interrupt (pour éviter une interruption inutile si plus rien à transmettre)
            PLIB_INT_SourceDisable(INT_ID_0, INT_SOURCE_USART_1_TRANSMIT);
        }
        // Clear the TX interrupt Flag (Seulement après TX)
        PLIB_INT_SourceFlagClear(INT_ID_0, INT_SOURCE_USART_1_TRANSMIT);
    }

    // Marque fin interruption avec Led3
    LED3_W = 0;
}
