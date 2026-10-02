// RCOM 2026/2027
//
// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define FALSE 0
#define TRUE 1

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BUF_SIZE 256

static int alarmEnabled = FALSE;

static void alarmHandler(int signal){
    alarmEnabled = FALSE;
}

static const unsigned char setFrame[5] = {
    0x7E,
    0x03,
    0x03,
    0x03 ^ 0x03,
    0x7E,
};

static const unsigned char uaFrame[5] = {
    0x7E,
    0x01,
    0x07,
    0x01 ^ 0x07,
    0x7E,
};

typedef enum{
    START,
    FLAG,
    ADDRESS,
    CONTROL,
    BCC,
    STOP
} State;

int readControlFrame(unsigned char expectedA, unsigned char expectedC){
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
                if (byte == 0x7E) state = FLAG;
                break;
            case FLAG:
                if (byte == expectedA) state = ADDRESS;
                else if (byte == 0x7E) state = FLAG;
                else state = START;
                break;
            case ADDRESS:
                if (byte == expectedC) state = CONTROL;
                else if (byte == 0x7E) state = FLAG;
                else state = START;
                break;
            case CONTROL:
                if (byte == (expectedA ^ expectedC)) state = BCC;
                else if (byte == 0x7E) state = FLAG;
                else state = START;
                break;
            case BCC:
                if (byte == 0x7E) state = STOP;
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
    // TODO: Implement this function
    int bytes = writeBytesSerialPort(buf, bufSize);
    printf("%d bytes written to serial port\n", bytes);

    return 0;
}

////////////////////////////////////////////////
// LLRECEIVE
////////////////////////////////////////////////
int llReceive(unsigned char *packet){
    // TODO: Implement this function

    return 0;
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
