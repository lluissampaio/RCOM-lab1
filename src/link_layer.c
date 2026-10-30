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

#define TX 0X03 
#define RX 0X01 

#define UA   0x07 //Replie
#define SET  0x03 //Command
#define I0   0X00 //Command
#define I1   0X80 //Command
#define RR0  0xAA //Replie
#define RR1  0xAB //Replie
#define REJ0 0x54 //Replie
#define REJ1 0x55 //Replie
#define DISC 0x0B //Command

#define FLAG 0x7E
#define ESC  0X7D

static LinkLayer linkConfig;

static int alarmEnabled = FALSE;

static unsigned char expectedFrame = I0; // for tx and rx

static void alarmHandler(int signal){
    alarmEnabled = FALSE;
}

static const unsigned char setFrame[5] = {
    FLAG,
    TX,
    SET,
    TX ^ SET,
    FLAG,
};

static const unsigned char uaFrame[5] = {
    FLAG,
    TX, // Default receiver responses
    UA,
    TX ^ UA,
    FLAG,
};

static int build_frame(unsigned char* res, const unsigned char adress, const unsigned char control){
    res[0]=FLAG; res[4]=FLAG;
    res[1]=adress;
    res[2]=control;
    res[3]=adress^control;
    return writeBytesSerialPort(res, 5);
}

static int readControlFrame(unsigned char *adress, unsigned char *control){
    typedef enum{START, FLAG_RCV, ADDRESS_RCV, CONTROL_RCV, BCC_RCV, STOP} State;
    State state = START;
    unsigned char byte;

    while (state != STOP){        
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
                state = ADDRESS_RCV;
                if (byte == FLAG) state = FLAG_RCV;
                else *adress = byte;
                break;
            case ADDRESS_RCV:
                state = CONTROL_RCV;    
                if (byte == FLAG) state = FLAG_RCV;
                else *control = byte;
                break;
            case CONTROL_RCV:
                if (byte == (*adress ^ *control)) state = BCC_RCV;
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

    linkConfig = llParameters;

    if (openSerialPort(llParameters.serialPort, llParameters.baudRate) < 0){
        perror("openSerialPort");
        return -1;
    }

    printf("Serial port %s opened\n", llParameters.serialPort);

    struct sigaction sa = {0};
    sa.sa_handler = &alarmHandler;
    sigaction(SIGALRM, &sa, NULL);

    unsigned char adress = 0,control = 0;

    for (int i = 0; i <= llParameters.nRetransmissions; i++){

        if (writeBytesSerialPort(setFrame, sizeof(setFrame)) < 0) {
            perror("writeBytesSerialPort");
            closeSerialPort();
            return -1;
        }

        alarm(llParameters.timeout);
        alarmEnabled = TRUE;

        while (alarmEnabled){
            if (readControlFrame(&adress,&control) == 0){
                if (adress==TX && control == uaFrame[2]){
                    alarm(0);
                    alarmEnabled=FALSE;
                    expectedFrame = I0;
                    return 0;
                }
            }
        }
        
    }

    alarm(0);
    alarmEnabled = FALSE;
    closeSerialPort();
    return -1;
}

int llOpenRx(LinkLayer llParameters){
    
    linkConfig = llParameters;

    if (openSerialPort(llParameters.serialPort, llParameters.baudRate) < 0){
        perror("openSerialPort");
        return -1;
    }

    printf("Serial port %s opened\n", llParameters.serialPort);

    unsigned char adress = 0,control = 0;

    while (adress != setFrame[1] || control != setFrame[2]){
        if (readControlFrame(&adress, &control) != 0) {
            closeSerialPort();
            return -1;
        }
    }

    if (writeBytesSerialPort(uaFrame, sizeof(uaFrame)) < 0) {
        perror("writeBytesSerialPort");
        closeSerialPort();
        return -1;
    }
    expectedFrame = I0;
    return 0;
}

////////////////////////////////////////////////
// LLSEND
////////////////////////////////////////////////
int llSend(const unsigned char *buf, int bufSize){
    
    unsigned char frame [2 * bufSize + 7]; 

    frame[0] = FLAG;
    frame[1] = TX;
    frame[2] = expectedFrame;
    frame[3] = TX ^ expectedFrame;
    
    unsigned char bcc2 = 0x00;
    int frameIndex = 4;
    for (int i = 0; i < bufSize; i++){
        bcc2 ^= buf[i];
        if (buf[i] == FLAG){
            frame[frameIndex++] = ESC;
            frame[frameIndex++] = 0x5E;
        }
        else if (buf[i] == ESC){
            frame[frameIndex++] = ESC;
            frame[frameIndex++] = 0x5D;
        }
        else{
            frame[frameIndex++] = buf[i];
        }
    }

    if (bcc2 == FLAG) {
        frame[frameIndex++] = ESC;
        frame[frameIndex++] = 0x5E;
    } else if (bcc2 == ESC) {
        frame[frameIndex++] = ESC;
        frame[frameIndex++] = 0x5D;
    } else {
        frame[frameIndex++] = bcc2;
    }
    
    frame[frameIndex] = FLAG;
    int frameSize = frameIndex + 1;
    
    unsigned char adress = 0,control = 0;
    unsigned char expectedRR  = expectedFrame == I0 ? RR1 : RR0;
    unsigned char expectedREJ = expectedFrame == I0 ? REJ0 : REJ1;
    
    for (int i = 0; i <= linkConfig.nRetransmissions; i++){

        if (writeBytesSerialPort(frame, frameSize) < 0) {
            perror("writeBytesSerialPort");
            return -1;
        }
        printf("llSend: Frame sent, waiting for response\n");
        
        alarm(linkConfig.timeout);
        alarmEnabled = TRUE;
        while (alarmEnabled) {
            if (readControlFrame(&adress,&control) == 0){
                if (adress != TX) continue;
                
                if (control == expectedRR){
                    expectedFrame = expectedFrame == I0 ? I1 : I0;
                    printf("llSend: RR received, frame acknowledged\n");
                    alarm(0);
                    alarmEnabled = FALSE;
                    return frameSize;
                }
                else if (control == expectedREJ){
                    printf("REJ received, retransmitting frame\n");
                    alarm(0);
                    alarmEnabled = FALSE;
                    break; // Retransmit the frame
                }
                // the rest of valid control frames are ignored, waiting for the expected RR or REJ      
            }             
        }
    }

    alarm(0);
    alarmEnabled = FALSE;
    
    return -1;
}

////////////////////////////////////////////////
// LLRECEIVE
////////////////////////////////////////////////
int llReceive(unsigned char *packet){
    typedef enum{START, FLAG_RCV, ADDRESS_RCV, CONTROL_RCV, BCC1_RCV, DATA, STOP} State;
    State state = START;
    unsigned char byte;
    unsigned char address, control, bcc2 = 0x00;
    unsigned char xor = 0x00, prev_xor = 0x00;
    bool escaped = false, dataError = false;
    unsigned char dup, curRR, curREJ;
    int atual = 0;
    unsigned char response[5];

    while (state != STOP){
        int bytes = readByteSerialPort(&byte);
        if (bytes < 0) {
            perror("readByteSerialPort");
            return -1;
        }
        if (bytes == 0) continue;

        switch (state){
            case START:
                if (byte == FLAG) state = FLAG_RCV;
                break;
            case FLAG_RCV:
                address = byte;
                if (byte == TX) state = ADDRESS_RCV;
                else if (byte == FLAG) state = FLAG_RCV;
                else state = START;
                break;

            case ADDRESS_RCV:
                if (byte == FLAG) state = FLAG_RCV;
                else if (byte == I0 || byte == I1 || byte == DISC || byte == SET) {
                    control = byte;
                    state = CONTROL_RCV;
                } else {
                    state = START;
                }
                break;

            case CONTROL_RCV:
                if (byte == (address ^ control)) state = BCC1_RCV;
                else if (byte == FLAG) state = FLAG_RCV;
                else state = START;
                break;

            case BCC1_RCV:
                if (control == DISC) {
                    if (byte == FLAG) { // edge case explained in application layer tx
                        if (build_frame(response, RX, DISC) < 0) {
                            perror("writeBytesSerialPort");
                            return -1;
                        }
                        unsigned char adress = 0, ctrl = 0;
                        if (readControlFrame(&adress, &ctrl) == 0) {
                            if (adress == RX && ctrl == uaFrame[2]) {
                                closeSerialPort();
                                return -2;
                            }
                            else {
                                printf("Unexpected control frame received\n");
                                return -1;
                            }
                        } else return -1;
                    } else {
                        state = START;
                    }
                }
                else if (control == SET) {
                    if (byte == FLAG) { // edge case if sender didn't receive the ua frame
                        if (writeBytesSerialPort(uaFrame, sizeof(uaFrame)) < 0) {
                            perror("writeBytesSerialPort");
                            return -1;
                        }
                    }
                    state = START;
                }
                else if (byte == FLAG) {
                    state = FLAG_RCV;
                }
                else {
                    atual=0;
                    prev_xor = 0;
                    xor=0;
                    bcc2 = 0;
                    state = DATA;
                    escaped = FALSE;
                    dataError = false;
                    dup = expectedFrame == I0 ? RR0 : RR1;
                    curRR = expectedFrame == I0 ? RR1 : RR0;
                    curREJ = (control == I0) ? REJ0 : REJ1;
                    if (byte == ESC) escaped = TRUE;
                    else {
                        packet[atual] = byte;
                        atual++;
                        xor = byte;
                        bcc2 = byte;
                    }
                }
                break;

            case DATA:
                if (byte == FLAG) {
                    if (atual > 0) {
                        atual--;
                        xor = prev_xor;
                    } else {
                        dataError = true;
                    }

                    // duplicado
                    if (expectedFrame != control) {
                        printf("llReceive: Duplicate frame received\n");
                        if (build_frame(response, TX, dup) < 0) {
                            perror("writeBytesSerialPort");
                            return -1;
                        }
                        state = START;
                        break;
                    }
                    if (!escaped && !dataError && xor == bcc2) {
                        printf("llReceive: Frame received successfully\n");
                        if (build_frame(response, TX, curRR) < 0) {
                            perror("writeBytesSerialPort");
                            return -1;
                        }
                        expectedFrame = expectedFrame == I0 ? I1 : I0;
                        return atual;
                    } else { // rejeitado
                        if (build_frame(response, TX, curREJ) < 0) {
                            perror("writeBytesSerialPort");
                            return -1;
                        }
                        state = START;
                    }
                    break;
                }

                if (escaped) {
                    escaped = false;
                    if (byte == 0x5E) byte = FLAG;
                    else if (byte == 0x5D) byte = ESC;
                    else {
                        dataError = true;
                        break;
                    }

                    prev_xor = xor;
                    xor ^= byte;
                    bcc2 = byte;
                    if (atual < MAX_PAYLOAD_SIZE) {
                        packet[atual] = byte;
                    }
                    atual++;
                } else {
                    if (byte == ESC) escaped = true;
                    else {
                        prev_xor = xor;
                        xor ^= byte;
                        bcc2 = byte;
                        if (atual < MAX_PAYLOAD_SIZE) {
                            packet[atual] = byte;
                        }
                        atual++;
                    }
                }

                if (atual > MAX_PAYLOAD_SIZE + 1) { // Flag perdida
                    if (expectedFrame != control) { // duplicado
                        if (build_frame(response, TX, dup) < 0) {
                            perror("writeBytesSerialPort");
                            return -1;
                        }
                    } else { // rejeitado
                        if (build_frame(response, TX, curREJ) < 0) {
                            perror("writeBytesSerialPort");
                            return -1;
                        }
                    }
                    state = START;
                }
                break;

            default:
                break;
        }
    }

    return -1;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llCloseTx(){
    
    unsigned char discFrame[5] = {FLAG,TX,DISC,TX ^ DISC,FLAG};

    unsigned char adress = 0,control = 0;
    bool discReceived = false;
    
    for (int i = 0; i <= linkConfig.nRetransmissions; i++){

        if (discReceived) break;
        
        if (writeBytesSerialPort(discFrame, 5) < 0){
            perror("writeBytesSerialPort");
            closeSerialPort();
            return -1;
        }

        alarm(linkConfig.timeout);
        alarmEnabled = TRUE;

        while (alarmEnabled){
            if (readControlFrame(&adress, &control) == 0){
                if (adress==RX && control == DISC){
                    unsigned char ua_frame[5] = {FLAG,RX,UA,RX ^ UA,FLAG};
                    if (writeBytesSerialPort(ua_frame, 5) < 0){
                        perror("writeBytesSerialPort");
                        alarm(0);
                        alarmEnabled = FALSE;
                        closeSerialPort();
                        return -1;
                    }
                    alarm(0);
                    alarmEnabled = FALSE;
                    discReceived = true;
                    break;
                }
            }
        }          
    
    }

    alarm(0);
    alarmEnabled = FALSE;
    usleep(50000);//no mundo real, tem de haver este delay, para a serial port nao fechar antes de os bytes serem enviados

    if (closeSerialPort() < 0){
        perror("closeSerialPort");
        return -1;
    }

    if (!discReceived) return -1;

    printf("Serial port closed\n");
    return 0;
}

int llCloseRx(){
    
    unsigned char adress = 0, control = 0;

    while (adress != TX || control != DISC) {
        if (readControlFrame(&adress, &control) != 0) {
            printf("readByteSerialPort failed\n");
            return -1;
        }
    }
    
    unsigned char discFrame[5] = {FLAG,RX,DISC,RX ^ DISC,FLAG};

    if (writeBytesSerialPort(discFrame, 5) < 0) {
        perror("writeBytesSerialPort");
        return -1;
    }

    while (adress != RX || control != UA) {
        if (readControlFrame(&adress, &control) != 0) {
            printf("readByteSerialPort failed\n");
            return -1;
        }
    }

    if (closeSerialPort() < 0) {
        perror("closeSerialPort");
        return -1;
    }

    printf("Serial port closed\n");
    return 0;
}
