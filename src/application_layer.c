// RCOM 2026/2027
//
// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer llParameters = {
        .baudRate = baudRate,
        .nRetransmissions = nTries,
        .timeout = timeout,
    };
    strcpy(llParameters.serialPort, serialPort);

    if (strcmp(role, "tx") == 0){
        
        if (llOpenTx(llParameters) < 0) return;

        // fazer o Control Packet
        char *controlPacket = malloc(strlen(filename) + 3);

        // Data packets
        const char *currentFile = filename;
        while (strlen(currentFile) > MAX_PAYLOAD_SIZE - 3) {
            char *packet = malloc(MAX_PAYLOAD_SIZE);
            if (packet == NULL) {
                llCloseTx();
                return;
            }
            packet[0] = 0x02;
            packet[1] = (MAX_PAYLOAD_SIZE - 3) / 256;
            packet[2] = (MAX_PAYLOAD_SIZE - 3) % 256;
            memcpy(packet + 3, currentFile, MAX_PAYLOAD_SIZE - 3);
            llSend((const unsigned char *)packet, MAX_PAYLOAD_SIZE);
            free(packet);
            currentFile += MAX_PAYLOAD_SIZE - 3;
        }

        int size = strlen(currentFile);
        if (size > 0) {
            char *packet = malloc(size + 3);
            if (packet == NULL) {
                llCloseTx();
                return;
            }
            packet[0] = 0x02;
            packet[1] = size / 256;
            packet[2] = (size % 256);
            memcpy(packet + 3, currentFile, size);
            llSend((const unsigned char *)packet, size + 3);
            free(packet);
        }

        //

        llCloseTx();
    }
    else if (strcmp(role, "rx") == 0){

        if (llOpenRx(llParameters) < 0) return;

        //llReceive(TODO);
        
        llCloseRx();
    }
    else{
        printf("Invalid role: %s. Must be 'tx' or 'rx'.\n", role);
        return;
    }
}
