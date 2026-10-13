// RCOM 2026/2027
//
// Link layer protocol implementation

#define _POSIX_SOURCE 1 // POSIX compliant source

#include "link_layer.h"
#include "serial_port.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>

#define FALSE 0
#define TRUE 1

// MISC
#define BUF_SIZE 256

#define FLAG 0x7E
#define ESC 0X7D

static int alarmEnabled = FALSE;

static void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
}

static const unsigned char setFrame[5] = {
    FLAG,
    0x03,
    0x03,
    0x03 ^ 0x03,
    FLAG,
};

static const unsigned char uaFrame[5] = {
    FLAG,
    0x01,
    0x07,
    0x01 ^ 0x07,
    FLAG,
};

static void response_frame(unsigned char *res, const unsigned char control, const bool error)
{
    res[0] = FLAG;
    res[1] = 0x03;
    res[4] = FLAG;
    if (control == 0x00)
        res[2] = error ? 0x54 : 0xAB;
    else if (control == 0x80)
        res[2] = error ? 0x55 : 0xAA;
    res[3] = res[1] ^ res[2];
}

static int readControlFrame(unsigned char expectedA, unsigned char expectedC)
{
    typedef enum
    {
        START,
        FLAG_RCV,
        ADDRESS_RCV,
        CONTROL_RCV,
        BCC_RCV,
        STOP
    } State;
    State state = START;
    unsigned char byte;

    while (state != STOP)
    {
        if (alarmEnabled == FALSE)
            return -1;

        int bytes = readByteSerialPort(&byte);
        if (bytes < 0)
        {
            if (alarmEnabled == FALSE)
                return -1;
            perror("readByteSerialPort");
            return -1;
        }
        if (bytes == 0)
            continue;

        switch (state)
        {
        case START:
            if (byte == FLAG)
                state = FLAG_RCV;
            break;
        case FLAG_RCV:
            if (byte == expectedA)
                state = ADDRESS_RCV;
            else if (byte == FLAG)
                state = FLAG_RCV;
            else
                state = START;
            break;
        case ADDRESS_RCV:
            if (byte == expectedC)
                state = CONTROL_RCV;
            else if (byte == FLAG)
                state = FLAG_RCV;
            else
                state = START;
            break;
        case CONTROL_RCV:
            if (byte == (expectedA ^ expectedC))
                state = BCC_RCV;
            else if (byte == FLAG)
                state = FLAG_RCV;
            else
                state = START;
            break;
        case BCC_RCV:
            if (byte == FLAG)
                state = STOP;
            else
                state = START;
            break;
        default:
            break;
        }
    }
    // =============================================================================
    // llReceive: lê uma trama I (F A C BCC1 D1..DN BCC2 F), tira o stuffing,
    // verifica o BCC2 e responde RR (ok) ou REJ (erro) ao transmissor.
    //
    // --- BUGS QUE TINHA ANTES E O QUE FOI CORRIGIDO ---
    //
    // 1) O primeiro byte de dados (D1) perdia-se.
    //    O estado BCC1_RCV só mudava para DATA sem guardar o byte.
    //    CORRECAO: no BCC1_RCV, no ramo else, passamos a guardar o D1
    //    (packet[atual]=byte; atual++) e a somá-lo ao XOR, como qualquer dado.
    //
    // 2) A FLAG final era tratada como se fosse dados.
    //    Faltava sair do case DATA quando chegava a FLAG, por isso ela era
    //    guardada no buffer e entrava no XOR.
    //    CORRECAO: quando byte==FLAG no DATA, tratamos logo o fim da trama e
    //    saímos com return, sem a processar como dado.
    //
    // 3) Pendurava no fim da trama.
    //    A verificação do BCC2 estava num estado à parte (BCC2_RCV) que só corria
    //    se chegasse mais um byte, mas depois da FLAG não vem nada.
    //    CORRECAO: a verificação do BCC2 passou para dentro do case DATA, feita no
    //    mesmo instante em que apanhamos a FLAG.
    //
    // 4) Numa trama boa, o RR nunca era enviado.
    //    O envio estava no case STOP, que nunca corria porque o while(state!=STOP)
    //    sai do ciclo assim que state==STOP.
    //    CORRECAO: o RR/REJ é enviado dentro do case DATA, antes do return, por isso
    //    corre sempre.
    //
    // --- O QUE ESTAS ADICOES FAZEM ---
    // Comparam o previousxor (XOR só dos dados D1..DN) com o BCC2 recebido:
    //   iguais  -> envia RR  e devolve o payload (return atual);
    //   difere  -> envia REJ e devolve -1.
    //
    // --- AINDA POR FAZER (sexta) ---
    //   1) STUFFING + BCC2: fazer o destuffing ANTES do XOR, somar o byte REAL
    //      (não o 0x7D/0x5E em cru), senão o BCC2 falha em tramas com stuffing.
    //   2) TRAMAS REPETIDAS (Ns): guardar o Ns esperado; se vier repetida,
    //      responder RR mas NAO entregar os dados outra vez.
    // =============================================================================

    return 0;
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llOpenTx(LinkLayer llParameters)
{

    if (openSerialPort(llParameters.serialPort, llParameters.baudRate) < 0)
    {
        perror("openSerialPort");
        return -1;
    }

    printf("Serial port %s opened\n", llParameters.serialPort);

    struct sigaction sa = {0};
    sa.sa_handler = &alarmHandler;
    sigaction(SIGALRM, &sa, NULL);

    for (int i = 0; i < llParameters.nRetransmissions; i++)
    {

        int bytes = writeBytesSerialPort(setFrame, sizeof(setFrame));
        printf("%d bytes written to serial port\n", bytes);

        alarm(llParameters.timeout);
        alarmEnabled = TRUE;

        if (readControlFrame(uaFrame[1], uaFrame[2]) == 0)
        {
            alarm(0);
            printf("UA recebido com sucesso!\n");
            return 0;
        }
    }

    return -1;
}

int llOpenRx(LinkLayer llParameters)
{

    if (openSerialPort(llParameters.serialPort, llParameters.baudRate) < 0)
    {
        perror("openSerialPort");
        return -1;
    }

    printf("Serial port %s opened\n", llParameters.serialPort);

    alarmEnabled = TRUE; // Por causa da função auxiliar

    if (readControlFrame(setFrame[1], setFrame[2]) == 0)
    {
        printf("SET recebido com sucesso!\n");
        writeBytesSerialPort(uaFrame, sizeof(uaFrame));
        printf("UA enviado de volta!\n");
        alarmEnabled = FALSE;
        return 0;
    }

    return -1;
}

////////////////////////////////////////////////
// LLSEND
////////////////////////////////////////////////
int llSend(const unsigned char *buf, int bufSize)
{
    // Implementar o byte stuffing
    // Implementar timeouts
    int bytes = writeBytesSerialPort(buf, bufSize);
    printf("%d bytes written to serial port\n", bytes);

    return 0;
}

////////////////////////////////////////////////
// LLRECEIVE
////////////////////////////////////////////////
int llReceive(unsigned char *packet)
{
    typedef enum
    {
        START,
        FLAG_RCV,
        ADDRESS_RCV,
        CONTROL_RCV,
        BCC1_RCV,
        DATA,
        BCC2_RCV,
        STOP,
        ERROR
    } State;
    State state = START;
    unsigned char byte;
    unsigned char address, control, bcc2;
    unsigned char xor = 0x00, previousxor = 0x00;
    int atual = 0;

    unsigned char *response = malloc(5);
    if (response == NULL)
    {
        return -1;
    }

    while (state != STOP)
    {
        int bytes = readByteSerialPort(&byte);
        if (bytes < 0)
        {
            perror("readByteSerialPort");
            free(response);
            return -1;
        }
        if (bytes == 0)
            continue;

        switch (state)
        {
        case START:
            if (byte == FLAG)
                state = FLAG_RCV;
            break;
        case FLAG_RCV:
            address = byte;
            if (byte == 0x03)
                state = ADDRESS_RCV;
            else if (byte == FLAG)
                state = FLAG_RCV;
            else
                state = START;
            break;
        case ADDRESS_RCV:
        {
            if (byte == 0x00 || byte == 0x80 || byte == 0x0B)
            {
                control = byte;
                state = CONTROL_RCV;
            }
            break;
        }
        case CONTROL_RCV:
            if (byte == (address ^ control))
                state = BCC1_RCV;
            else
                state = ERROR;
            break;
        case BCC1_RCV:
            if (control == 0x0B && byte == FLAG)
            {
                state = STOP;
            }
            else
            {
                previousxor = xor;
                xor = xor ^ byte;
                packet[atual] = byte; /*------------------*/
                atual++;
                state = DATA;
            }
            break;
        case DATA:
            if (byte == FLAG)
            {
                unsigned char bcc2_rx = packet[atual - 1];
                atual--;
                if (previousxor == bcc2_rx)
                {
                    response_frame(response, control, false);
                    writeBytesSerialPort(response, 5);
                    free(response);
                    return atual;
                }
                else
                {
                    response_frame(response, control, true);
                    writeBytesSerialPort(response, 5);
                    free(response);
                    return -1;
                }
            }
            previousxor = xor;
            xor = xor ^ byte;
            if (atual > 0 && packet[atual - 1] == ESC && byte == 0x5E)
            {
                packet[atual - 1] = FLAG;
            }
            else if (atual > 0 && packet[atual - 1] == ESC && byte == 0x5D)
            {
                packet[atual - 1] = ESC;
            }
            else
            {
                packet[atual] = byte;
                atual++;
            }
            break;
        case BCC2_RCV:
            if (previousxor == bcc2)
            {
                packet[atual - 1] = '\0';
                state = STOP;
            }
            else
                state = ERROR;
            break;
        case ERROR:
            if (control != 0x0B)
            {
                response_frame(response, control, true);
                writeBytesSerialPort(response, 5);
            }
            free(response);
            return -1;
        case STOP:
            if (control != 0x0B)
            {
                response_frame(response, control, false);
                writeBytesSerialPort(response, 5);
            }
            break;
        default:
            break;
        }
    }
    free(response);
    return atual;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llCloseTx()
{
    if (closeSerialPort() < 0)
    {
        perror("closeSerialPort");
        return -1;
    }

    printf("Serial port closed\n");

    return 0;
}

int llCloseRx()
{
    if (closeSerialPort() < 0)
    {
        perror("closeSerialPort");
        return -1;
    }

    printf("Serial port closed\n");

    return 0;
}
