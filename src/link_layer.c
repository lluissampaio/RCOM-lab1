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

int alarmEnabled = FALSE;

void alarmHandler(int signal)
{
    alarmEnabled = FALSE;

}

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BUF_SIZE 256

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

    for (int i = 0; i < llParameters.nRetransmissions; i++)
    {
        unsigned char set[5] = {
            0x7E,
            0x03,
            0x03,
            0x03 ^ 0x03,
            0x7E,
        };

        int bytes = writeBytesSerialPort(set, sizeof(set));
        printf("%d bytes written to serial port\n", bytes);

        sigaction(SIGALRM, &sa, NULL);
        alarm(llParameters.timeout);
        alarmEnabled = TRUE;

        unsigned char ua[5] = {0};
        int index = 0;
        unsigned char byte;

        while (1)
        {
            if (alarmEnabled == FALSE)
            {
                printf("Timeout waiting for UA after %d seconds\n", llParameters.timeout);
                alarm(0);
                break;
            }

            int bytes = readByteSerialPort(&byte);

            if (bytes < 0)
            {
                perror("readByteSerialPort");
                alarm(0);
                return -1;
            }

            if (bytes == 0)
            {
                continue;
            }

            if (byte == 0x7E && index == 0)
            {
                ua[index] = byte;
                index++;
                continue;
            }

            if (index > 0)
            {
                ua[index] = byte;
                index++;

                if (index == 5)
                {
                    if (ua[0] == 0x7E && ua[4] == 0x7E &&
                        ua[1] == 0x01 &&
                        (ua[1] ^ ua[2]) == ua[3])
                    {
                        printf("UA received: %02X %02X %02X %02X %02X\n",
                               ua[0], ua[1], ua[2], ua[3], ua[4]);
                        alarm(0);
                        return 0;
                    }

                    index = 0;
                    memset(ua, 0, sizeof(ua));
                }
            }
        }
    }

    unsigned char buf[5] = {
        0x7E,
        0x03,
        0x03,
        0x03 ^ 0x03,
        0x7E,
    };

    int bytes = writeBytesSerialPort(buf, sizeof(buf));

    return 0;
}

int llOpenRx(LinkLayer llParameters)
{
    if (openSerialPort(llParameters.serialPort, llParameters.baudRate) < 0)
    {
        perror("openSerialPort");
        return -1;
    }

    printf("Serial port %s opened\n", llParameters.serialPort);

    unsigned char frame[5] = {0};
    int index = 0;
    unsigned char byte;

    while (1)
    {
        int bytes = readByteSerialPort(&byte);

        if (bytes < 0)
        {
            perror("readByteSerialPort");
            return -1;
        }

        if (bytes == 0)
        {
            continue;
        }

        if (byte == 0x7E && index == 0)
        {
            frame[index] = byte;
            index++;
            continue;
        }

        if (index > 0)
        {
            frame[index] = byte;
            index++;

            if (index == 5)
            {
                if (frame[0] == 0x7E && frame[4] == 0x7E &&
                    frame[1] == 0x03 &&
                    (frame[1] ^ frame[2]) == frame[3])
                {
                    printf("Trama recebida: %02X %02X %02X %02X %02X\n",
                           frame[0], frame[1], frame[2], frame[3], frame[4]);

                    unsigned char ua[5] = {
                        0x7E,
                        0x01,
                        0x07,
                        0x01 ^ 0x07,
                        0x7E,
                    };

                    writeBytesSerialPort(ua, sizeof(ua));
                    printf("UA enviado\n");
                    break;
                }

                index = 0;
                memset(frame, 0, sizeof(frame));
            }
        }
    }

    return 0;
}

////////////////////////////////////////////////
// LLSEND
////////////////////////////////////////////////
int llSend(const unsigned char *buf, int bufSize)
{
    // TODO: Implement this function
    int bytes = writeBytesSerialPort(buf, bufSize);
    printf("%d bytes written to serial port\n", bytes);

    return 0;
}

////////////////////////////////////////////////
// LLRECEIVE
////////////////////////////////////////////////
int llReceive(unsigned char *packet)
{
    // TODO: Implement this function

    return 0;
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
