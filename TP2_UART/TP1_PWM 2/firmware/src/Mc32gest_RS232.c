// Mc32Gest_RS232.C
// Canevas manipulatio TP2 RS232 SLO2 2017-18
// Fonctions d'�mission et de r�ception des message
// CHR 20.12.2016 ajout traitement int error
// CHR 22.12.2016 evolution des marquers observation int Usart
// SCA 03.01.2018 nettoy� r�ponse interrupt pour ne laisser que les 3 ifs

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
typedef union {
        uint16_t val;
        struct {uint8_t lsb;
                uint8_t msb;} shl;
} U_manip16;


// Definition pour les messages
#define MESS_SIZE  5
// avec int8_t besoin -86 au lieu de 0xAA
#define STX_code  (-86)

// Structure d�crivant le message
typedef struct {
    uint8_t Start;
    uint8_t  Speed;
    uint8_t  Angle;
    uint8_t MsbCrc;
    uint8_t LsbCrc;
} StruMess;


// Struct pour �mission des messages
StruMess TxMess;
// Struct pour r�ception des messages
StruMess RxMess;

// Declaration des FIFO pour r�ception et �mission
#define FIFO_RX_SIZE ( (4*MESS_SIZE) + 1)  // 4 messages
#define FIFO_TX_SIZE ( (4*MESS_SIZE) + 1)  // 4 messages

int8_t fifoRX[FIFO_RX_SIZE];
// Declaration du descripteur du FIFO de r�ception
S_fifo descrFifoRX;


int8_t fifoTX[FIFO_TX_SIZE];
// Declaration du descripteur du FIFO d'�mission
S_fifo descrFifoTX;


// Initialisation de la communication s�rielle
void InitFifoComm(void)
{    
    // Initialisation du fifo de r�ception
    InitFifo ( &descrFifoRX, FIFO_RX_SIZE, fifoRX, 0 );
    // Initialisation du fifo d'�mission
    InitFifo ( &descrFifoTX, FIFO_TX_SIZE, fifoTX, 0 );
    
    // Init RTS 
    RS232_RTS = 1;   // interdit �mission par l'autre
   
} // InitComm

 
// *****************************************************************************
/* Fonction :
    int GetMessage(S_pwmSettings *pData)

  R�sum� :
    Cette fonction g�re la r�ception de messages depuis une FIFO s�rie.
    Elle r�cup�re les donn�es de la FIFO s�rie, v�rifie l'int�grit� du message
    � l'aide d'un calcul de CRC, et met � jour la structure de param�tres PWM
    (pData) en cons�quence si le message est valide.

  Description :
    La fonction commence par v�rifier le nombre de caract�res disponibles
    dans la FIFO s�rie. Si le nombre de caract�res est suffisant et que le
    premier octet (Start byte) du message est correct, la fonction extrait
    les champs du message (Speed et Angle), calcul le CRC, et v�rifie
    l'int�grit� du message. Si le CRC est correct, la fonction met � jour
    les param�tres de vitesse et d'angle dans la structure pData, et retourne
    un statut indiquant qu'un message a �t� re�u avec succ�s.

    Si le message est incorrect (CRC incorrect ou Start byte incorrect),
    la fonction clignote une LED pour signaler une erreur, incr�mente un
    compteur de cycles de r�ception, et ne met pas � jour la structure pData.
    Si aucun message valide n'est re�u pendant plusieurs cycles, la fonction
    r�initialise le statut de communication.

    La gestion du contr�le de flux de r�ception est �galement effectu�e
    pour autoriser ou d�sautoriser l'�mission par l'autre partie.

  Param�tres :
    - pData : Un pointeur vers la structure de param�tres PWM (S_pwmSettings)
              contenant les informations de vitesse et d'angle.

  Valeur de retour :
    - 0 : Aucun message valide re�u, les donn�es locales (pData) restent inchang�es.
    - 1 : Un message valide a �t� re�u et les donn�es locales (pData) ont �t� mises � jour.

*/
int GetMessage(S_pwmSettings *pData) {
    static uint8_t commStatus = 0;  // Statut de la communication (0 = pas de message, 1 = message re�u)
    uint8_t NbCharToRed = 0;  // Nombre de caract�res � lire dans la FIFO
    static int IcycleRx = 0;  // Compteur de cycles de r�ception
    uint16_t ValCrc;  // Valeur du CRC

    // V�rifier le nombre de caract�res � lire dans la FIFO s�rie
    NbCharToRed = GetReadSize(&descrFifoRX);

    // Lire le premier octet de la FIFO (Start byte)
    GetCharFromFifo(&descrFifoRX, &RxMess.Start);

    // Si le nombre de caract�res est suffisant et le Start byte est correct (0xAA)
    if ((NbCharToRed >= MESS_SIZE) & (RxMess.Start == 0xAA)) {
        ValCrc = 0xFFFF;  // Initialiser la valeur du CRC

        // Mettre � jour le CRC avec le Start byte
        ValCrc = updateCRC16(ValCrc, RxMess.Start);

        // Lire le champ Speed et mettre � jour le CRC
        GetCharFromFifo(&descrFifoRX, &RxMess.Speed);
        ValCrc = updateCRC16(ValCrc, RxMess.Speed);

        // Lire le champ Angle et mettre � jour le CRC
        GetCharFromFifo(&descrFifoRX, &RxMess.Angle);
        ValCrc = updateCRC16(ValCrc, RxMess.Angle);

        // Lire les octets du CRC (MSB et LSB) et mettre � jour le CRC
        GetCharFromFifo(&descrFifoRX, &RxMess.MsbCrc);
        ValCrc = updateCRC16(ValCrc, RxMess.MsbCrc);
        GetCharFromFifo(&descrFifoRX, &RxMess.LsbCrc);
        ValCrc = updateCRC16(ValCrc, RxMess.LsbCrc);

        ValCrc = ValCrc;  // Ignorer la valeur finale du CRC (cette ligne semble inutile)

        // V�rifier si le CRC est correct
        if (ValCrc == 0) {
            // Mettre � jour les valeurs de vitesse et d'angle dans la structure pData
            pData->SpeedSetting = RxMess.Speed;
            pData->AngleSetting = RxMess.Angle;

            // Mettre � jour le statut de communication
            commStatus = 1;
            IcycleRx = 0;
        } else {
            // Clignoter une LED pour signaler une erreur CRC
            LED6_W = !LED6_R;
            
            // R�initialiser le statut de communication et incr�menter le compteur de cycles
            commStatus = 0;
            IcycleRx++;
        }
    } else {
        // Si le nombre de caract�res � lire est insuffisant, ou si le Start byte est incorrect,
        // g�rer le compteur de cycles de r�ception
        if (IcycleRx >= 10) {
            commStatus = 0;
            IcycleRx = 0;
        } else {
            IcycleRx++;
        }
    }
    // Gestion du contr�le de flux de la r�ception
    if (GetWriteSpace(&descrFifoRX) >= (2 * MESS_SIZE)) {
        // Autoriser l'�mission par l'autre (abaisser le signal RTS)
        RS232_RTS = 0;
    }
    return commStatus;  // Retourner le statut de communication
} // GetMessage


// *****************************************************************************
/* Fonction :
    void SendMessage(S_pwmSettings *pData)

  R�sum� :
    Cette fonction g�re l'envoi cyclique de messages via une FIFO s�rie.
    Elle compose un message avec les param�tres de vitesse et d'angle contenus
    dans la structure pData, calcule le CRC, et d�pose le message dans la FIFO
    d'�mission. Elle g�re �galement le contr�le de flux en autorisant ou
    d�sautorisant l'�mission en fonction de l'�tat du signal CTS (Clear To Send).

  Description :
    La fonction commence par v�rifier si suffisamment d'espace est disponible
    dans la FIFO s�rie pour �crire un message complet. Si c'est le cas, elle
    compose le message en utilisant le format d�fini, y compris le Start byte,
    les champs Speed et Angle, ainsi que les octets du CRC. Elle utilise la
    fonction updateCRC16 pour calculer le CRC.

    Ensuite, la fonction d�pose chaque octet du message dans la FIFO d'�mission
    en utilisant la fonction PutCharInFifo.

    Enfin, la fonction g�re le contr�le de flux. Si le signal CTS est bas (0)
    et qu'il y a au moins un caract�re � envoyer dans la FIFO d'�mission, elle
    autorise l'interruption d'�mission pour d�clencher l'envoi du message.

  Param�tres :
    - pData : Un pointeur vers la structure de param�tres PWM (S_pwmSettings)
              contenant les informations de vitesse et d'angle.

*/
void SendMessage(S_pwmSettings *pData)
{
    int8_t freeSize;
    U_manip16 valCrc16;
    valCrc16.val = 0xFFFF;

    // Test si place pour �crire 1 message
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

        // D�pose le message dans la FIFO d'�mission
        PutCharInFifo(&descrFifoTX, TxMess.Start);
        PutCharInFifo(&descrFifoTX, TxMess.Speed);
        PutCharInFifo(&descrFifoTX, TxMess.Angle);
        PutCharInFifo(&descrFifoTX, TxMess.MsbCrc);
        PutCharInFifo(&descrFifoTX, TxMess.LsbCrc);
    }

    // Gestion du contr�le de flux
    freeSize = GetReadSize(&descrFifoTX);

    if ((RS232_CTS == 0) && (freeSize > 0))
    {
        // Autorise l'interruption d'�mission
        PLIB_INT_SourceEnable(INT_ID_0, INT_SOURCE_USART_1_TRANSMIT);
    }
}



// *****************************************************************************
/* Routine d'interruption USART1

  R�sum� :
    Cette routine g�re les interruptions li�es � l'USART1, incluant les erreurs,
    la r�ception (RX), et la transmission (TX). Elle effectue le traitement
    appropri� en fonction de la nature de l'interruption.

  Description :
    La routine commence par marquer le d�but de l'interruption avec l'allumage
    de la LED3.

    Ensuite, elle v�rifie si l'interruption est li�e � une erreur. Si c'est le cas,
    elle traite l'erreur en lisant et en jetant les caract�res du buffer de r�ception.

    En cas d'interruption de r�ception (RX), elle lit les caract�res du buffer
    mat�riel de r�ception et les d�pose dans la FIFO de r�ception (descrFifoRX). Elle
    active �galement la LED4 � chaque r�ception de caract�re. Elle g�re �galement
    le contr�le de flux en ajustant la sortie RS232_RTS en fonction de la place
    disponible dans la FIFO de r�ception.

    Pour l'interruption de transmission (TX), elle envoie les caract�res de la FIFO
    de transmission (descrFifoTX) vers le buffer mat�riel de transmission tant que
    les conditions d'�mission sont remplies (CTS = 0, FIFO non vide, buffer d'�mission non plein).
    La LED5 est activ�e � chaque �mission de caract�re. Elle d�sactive l'interruption de
    transmission lorsque tous les caract�res ont �t� �mis.

    Finalement, la routine marque la fin de l'interruption avec l'extinction de la LED3.

  Note :
    Cette routine assume que la gestion des FIFOs, des LEDs, des signaux de contr�le
    (RS232_RTS et RS232_CTS) ainsi que l'initialisation des p�riph�riques USART1 et des
    interruptions ont �t� correctement configur�es ailleurs dans le code.

*/
void __ISR(_UART_1_VECTOR, ipl5AUTO) _IntHandlerDrvUsartInstance0(void)
{
    USART_ERROR UsartStatus;
    int8_t i_cts;
    uint8_t TXsize, freeSize;
    BOOL TxBuffFull;
    uint8_t c;

    // Marque d�but interruption avec Led3
    LED3_W = 1;

    // Is this an Error interrupt ?
    if (PLIB_INT_SourceFlagGet(INT_ID_0, INT_SOURCE_USART_1_ERROR) &&
        PLIB_INT_SourceIsEnabled(INT_ID_0, INT_SOURCE_USART_1_ERROR))
    {
        /* Clear pending interrupt */
        PLIB_INT_SourceFlagClear(INT_ID_0, INT_SOURCE_USART_1_ERROR);
        // Traitement de l'erreur � la r�ception.
        while (PLIB_USART_ReceiverDataIsAvailable(USART_ID_1))
        {
            PLIB_USART_ReceiverByteReceive(USART_ID_1);
        }
    }

    // Is this an RX interrupt ?
    if (PLIB_INT_SourceFlagGet(INT_ID_0, INT_SOURCE_USART_1_RECEIVE) &&
        PLIB_INT_SourceIsEnabled(INT_ID_0, INT_SOURCE_USART_1_RECEIVE))
    {

        // Oui Test si erreur parit� ou overrun
        UsartStatus = PLIB_USART_ErrorsGet(USART_ID_1);

        if ((UsartStatus & (USART_ERROR_PARITY |
                            USART_ERROR_FRAMING | USART_ERROR_RECEIVER_OVERRUN)) == 0)
        {

            // Lecture des caract�res depuis le buffer HW -> fifo SW
            // pour savoir s'il y a une data dans le buffer HW RX
            while (PLIB_USART_ReceiverDataIsAvailable(USART_ID_1))
            {
                // Traitement RX � faire ICI
                // Lecture des caract�res depuis le buffer HW -> fifo SW
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

        // Traitement controle de flux reception � faire ICI
        // Gerer sortie RS232_RTS en fonction de place dispo dans fifo reception
        freeSize = GetWriteSpace(&descrFifoRX);
        if (freeSize <= 6) // a cause d'un int pour 6 char
        {
            // Demande de ne plus �mettre
            RS232_RTS = 1;
        }
    } // end if RX

    // Is this a TX interrupt ?
    if (PLIB_INT_SourceFlagGet(INT_ID_0, INT_SOURCE_USART_1_TRANSMIT) &&
        PLIB_INT_SourceIsEnabled(INT_ID_0, INT_SOURCE_USART_1_TRANSMIT))
    {

        // Avant d'�mettre, on v�rifie 3 conditions :
        //  Si CTS = 0 autorisation d'�mettre (entr�e RS232_CTS)
        //  S'il y a un carat�res � �mettre dans le fifo
        //  S'il y a de la place dans le buffer d'�mission
        i_cts = RS232_CTS;
        TXsize = GetReadSize(&descrFifoTX);
        TxBuffFull = PLIB_USART_TransmitterBufferIsFull(USART_ID_1);

        if ((i_cts == 0) && (TXsize > 0) && TxBuffFull == false )
        {
            do
            {
                // Traitement TX � faire ICI
                // Envoi des caract�res depuis le fifo SW -> buffer HW
                GetCharFromFifo(&descrFifoTX, &c);
                PLIB_USART_TransmitterByteSend(USART_ID_1, c);
                LED5_W = !LED5_R; // Toggle Led5
                i_cts = RS232_CTS;
                TXsize = GetReadSize(&descrFifoTX);
                TxBuffFull = PLIB_USART_TransmitterBufferIsFull(USART_ID_1);
            } while ((i_cts == 0) && (TXsize > 0) && TxBuffFull == false);

            // disable TX interrupt (pour �viter une interruption inutile si plus rien � transmettre)
            PLIB_INT_SourceDisable(INT_ID_0, INT_SOURCE_USART_1_TRANSMIT);
        }
        // Clear the TX interrupt Flag (Seulement apr�s TX)
        PLIB_INT_SourceFlagClear(INT_ID_0, INT_SOURCE_USART_1_TRANSMIT);
    }

    // Marque fin interruption avec Led3
    LED3_W = 0;
}
