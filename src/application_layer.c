// RCOM 2026/2027
//
// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

#include <stdio.h>
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

    if (strcmp(role, "tx") == 0)
    {
        if (llOpenTx(llParameters) < 0)
        {
            return;
        }

        unsigned char frame[5] = {
            0x7E,
            0x03,
            0x07,
            0x03 ^ 0x07,
            0x7E,
        };

        llSend(frame, sizeof(frame));
        llCloseTx();
    }
    else if (strcmp(role, "rx") == 0)
    {
        unsigned char packet[5] = {0};

        if (llOpenRx(llParameters) < 0)
        {
            return;
        }

        llReceive(packet);
        llCloseRx();
    }
    else
    {
        printf("Invalid role: %s. Must be 'tx' or 'rx'.\n", role);
        return;
    }
}
