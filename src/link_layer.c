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
#define ESC  0X7D

static int alarmEnabled = FALSE;

static void alarmHandler(int signal){
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

static void response_frame(unsigned char* res, const unsigned char control, const bool error){
    res[0]= FLAG;
    res[1]= 0x03;
    res[4]= FLAG;
    if (control==0x00) res[2] = error ? 0x54 : 0xAB;
    else if (control==0x80) res[2] = error ? 0x55 : 0xAA;
    res[3] = res[1] ^ res[2];
}

static int readControlFrame(unsigned char expectedA, unsigned char expectedC){
    typedef enum{START, FLAG_RCV, ADDRESS_RCV, CONTROL_RCV, BCC_RCV, STOP} State;
    State state = START;
    unsigned char byte;

    while (state != STOP){
        if (alarmEnabled == FALSE) return -1;
        
        int bytes = readByteSerialPort(&byte);
        if (bytes < 0) {
            if (alarmEnabled == FALSE) return -1; 
            perror("readByteSerialPort");
            return -1;
        }
        if (bytes == 0) continue;

        switch (state){
            case START:
                if (byte == FLAG) state = FLAG_RCV;
                break;
            case FLAG_RCV:
                if (byte == expectedA) state = ADDRESS_RCV;
                else if (byte == FLAG) state = FLAG_RCV;
                else state = START;
                break;
            case ADDRESS_RCV:
                if (byte == expectedC) state = CONTROL_RCV;
                else if (byte == FLAG) state = FLAG_RCV;
                else state = START;
                break;
            case CONTROL_RCV:
                if (byte == (expectedA ^ expectedC)) state = BCC_RCV;
                else if (byte == FLAG) state = FLAG_RCV;
                else state = START;
                break;
            case BCC_RCV:
                if (byte == FLAG) state = STOP;
                else state = START;
                break;
            default:
                break;
        }
    }

    return 0;
}


////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llOpenTx(LinkLayer llParameters){

    if (openSerialPort(llParameters.serialPort, llParameters.baudRate) < 0){
        perror("openSerialPort");
        return -1;
    }

    printf("Serial port %s opened\n", llParameters.serialPort);

    struct sigaction sa = {0};
    sa.sa_handler = &alarmHandler;
    sigaction(SIGALRM, &sa, NULL);

    for (int i = 0; i < llParameters.nRetransmissions; i++){

        int bytes = writeBytesSerialPort(setFrame, sizeof(setFrame));
        printf("%d bytes written to serial port\n", bytes);

        alarm(llParameters.timeout);
        alarmEnabled = TRUE;

        if (readControlFrame(uaFrame[1], uaFrame[2]) == 0){
            alarm(0);
            printf("UA recebido com sucesso!\n");
            return 0;
        }        
        
    }

    return -1;
}

int llOpenRx(LinkLayer llParameters){
    
    if (openSerialPort(llParameters.serialPort, llParameters.baudRate) < 0){
        perror("openSerialPort");
        return -1;
    }

    printf("Serial port %s opened\n", llParameters.serialPort);

    alarmEnabled = TRUE; // Por causa da função auxiliar

    if (readControlFrame(setFrame[1], setFrame[2]) == 0) {
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
int llSend(const unsigned char *buf, int bufSize){
    // Implementar o byte stuffing
    // Implementar timeouts
    int bytes = writeBytesSerialPort(buf, bufSize);
    printf("%d bytes written to serial port\n", bytes);

    return 0;
}

////////////////////////////////////////////////
// LLRECEIVE
////////////////////////////////////////////////
int llReceive(unsigned char *packet){
    typedef enum{START, FLAG_RCV, ADDRESS_RCV, CONTROL_RCV, BCC1_RCV, DATA, BCC2_RCV, STOP, ERROR} State;
    State state = START;
    unsigned char byte;
    unsigned char address, control, bcc2;
    unsigned char xor = 0x00, previousxor = 0x00;
    int atual=0;
    
    unsigned char *response = malloc(5);
    if (response == NULL) {
        return -1;
    }

    while (state != STOP){
        int bytes = readByteSerialPort(&byte);
        if (bytes < 0) {
            perror("readByteSerialPort");
            free(response);
            return -1;
        }
        if (bytes == 0) continue;

        switch (state){
            case START:
                if (byte == FLAG) state = FLAG_RCV;
                break;
            case FLAG_RCV:
                address = byte;
                if (byte == 0x03) state = ADDRESS_RCV;
                else if (byte == FLAG) state = FLAG_RCV;
                else state = START;
                break;
            case ADDRESS_RCV:{
                if (byte == 0x00 || byte == 0x80 || byte == 0x0B){
                    control = byte;
                    state = CONTROL_RCV;
                }
                break;    
            }
            case CONTROL_RCV:
                if (byte == (address ^ control)) state = BCC1_RCV;
                else state = ERROR;
                break;
            case BCC1_RCV:
                if (control == 0x0B && byte == FLAG) state = STOP;
                else state = DATA;
                break;
            case DATA:
                if (byte == FLAG){
                    bcc2 = packet[atual-1];
                    state = BCC2_RCV;
                }
                previousxor = xor;
                xor=xor^byte;
                if (atual > 0 && packet[atual-1] == ESC && byte == 0x5E){
                    packet[atual-1] = FLAG;
                }
                else if (atual >0 && packet[atual-1] == ESC && byte == 0x5D){
                    packet[atual-1] = ESC;
                }
                else{
                    packet[atual] = byte;
                    atual++;
                }
                break;
            case BCC2_RCV:
                if (previousxor == bcc2){
                    packet[atual-1]='\0';
                    state = STOP;
                }
                else state = ERROR;
                break;
            case ERROR:
                if (control != 0x0B){
                    response_frame(response,control,true);
                    writeBytesSerialPort(response, 5);
                }
                free(response);
                return -1;
            case STOP:
                if (control != 0x0B){
                    response_frame(response,control,false);
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
int llCloseTx(){
    if (closeSerialPort() < 0){
        perror("closeSerialPort");
        return -1;
    }

    printf("Serial port closed\n");

    return 0;
}

int llCloseRx(){
    if (closeSerialPort() < 0){
        perror("closeSerialPort");
        return -1;
    }

    printf("Serial port closed\n");

    return 0;
}
