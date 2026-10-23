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

    if (strcmp(role, "tx") == 0)
    {

        if (llOpenTx(llParameters) < 0)
            return;
        // TLV 1
        FILE *fp = fopen(filename, "rb");
        if (fp == NULL)
        {
            llCloseTx();
            return;
        }
        fseek(fp, 0, SEEK_END);
        long sizer = ftell(fp); // V (value), valor do tamanho do file | podemos tentar dinamicamente sem ser long?
        rewind(fp);
        int BytesSizes = sizeof(long); // L (length), quantos bytes ocupa na memória
        int ControlPacketSize = 1 + 1 + 1 + BytesSizes + 1 + 1 + strlen(filename);
        unsigned char *buffer = malloc(ControlPacketSize);
        int pos = 0;
        buffer[pos++] = 0x01;                     // C
        buffer[pos++] = 0x00;                     // T1
        buffer[pos++] = BytesSizes;               // L1
        for (int i = BytesSizes - 1; i >= 0; i--) // V1
        {
            buffer[pos++] = (sizer >> (i * 8)) & 0xFF;
        }
        buffer[pos++] = 0x01;                             // T2
        buffer[pos++] = strlen(filename);                 // L2
        memcpy(buffer + pos, filename, strlen(filename)); // V2
        llSend(buffer, ControlPacketSize);
        free(buffer);

        // TLV2 / Data packets
        unsigned char *buffer_2 = malloc(MAX_PAYLOAD_SIZE);
        while (1)
        {
            int bytesRead = fread(buffer_2 + 3, 1, MAX_PAYLOAD_SIZE - 3, fp);
            if (bytesRead <= 0)
            {
                break;
            }
            buffer_2[0] = 0x02;
            buffer_2[1] = bytesRead / 256;
            buffer_2[2] = bytesRead % 256;
            llSend(buffer_2, bytesRead + 3);
        }
        free(buffer_2);

        // TLV3
        buffer = malloc(ControlPacketSize);
        pos = 0;
        buffer[pos++] = 0x03;                     // C
        buffer[pos++] = 0x00;                     // T1
        buffer[pos++] = BytesSizes;               // L1
        for (int i = BytesSizes - 1; i >= 0; i--) // V1
        {
            buffer[pos++] = (sizer >> (i * 8)) & 0xFF;
        }
        buffer[pos++] = 0x01;                             // T2
        buffer[pos++] = strlen(filename);                 // L2
        memcpy(buffer + pos, filename, strlen(filename)); // V2
        llSend(buffer, ControlPacketSize);
        free(buffer);
        fclose(fp);

        llCloseTx();
    }
    else if (strcmp(role, "rx") == 0)
    {

        if (llOpenRx(llParameters) < 0)
            return;

        // TODO, pensar se acrescento parametros
        // TODO, testar

        typedef struct
        {
            char *filename; // Opcional mandar
            int filesize;   // Obrigatorio mandar
            unsigned char *data;
        } File;

        File file = {NULL, -1, NULL};

        unsigned char *buf = malloc(MAX_PAYLOAD_SIZE);
        if (buf == NULL)
        {
            llCloseRx();
            return;
        }
        int size_atual = 0, size_value = 0, size_buf = 0, acc = 0;
        int index = 1;
        unsigned char finished = 0;
        while (!finished)
        {
            size_buf = llReceive(buf);
            if (size_buf == -2)
            {
                printf("Connection closed by transmitter\n");
                free(buf);
                llCloseRx();
                return;
            }
            if (size_buf < 0)
            {
                perror("llReceive");
                free(buf);
                llCloseRx();
                return;
            }
            if (size_buf == 0)
                continue;
            if (file.data == NULL && file.filesize >= 0)
            {
                file.data = malloc(file.filesize);
                if (file.data == NULL)
                {
                    printf("Failing doing mem allocation to File Data\n");
                    free(buf);
                    llCloseRx();
                    return;
                }
            }
            index = 1;
            switch (buf[0])
            {
            case 1:
                while (index < size_buf)
                {
                    size_value = buf[index + 1];
                    if (buf[index] == 0)
                    {
                        file.filesize = 0;
                        for (int i = 0; i < size_value; i++)
                        {
                            file.filesize = (file.filesize << 8) | buf[index + 2 + i];
                        }
                    }
                    else if (buf[index] == 1)
                    {
                        file.filename = malloc(size_value + 1);
                        memcpy(file.filename, buf + index + 2, size_value);
                        file.filename[size_value] = '\0';
                    }
                    else
                    {
                        printf("Invalid control packet\n");
                        free(buf);
                        llCloseRx();
                        return;
                    }
                    index += size_value + 2;
                }
                break;
            case 2:
                if (file.filesize == -1)
                {
                    printf("The file size must be indicated before sending data\n");
                    free(buf);
                    llCloseRx();
                    return;
                }
                size_atual = 256 * buf[1] + buf[2];
                if (acc + size_atual > file.filesize)
                {
                    printf("The file data is bigger than the file size initial planned\n");
                    free(buf);
                    llCloseRx();
                    return;
                }
                memcpy(file.data + acc, buf + 3, size_atual);
                acc += size_atual;
                break;
            case 3:
                if (file.filesize != acc)
                {
                    printf("File data doesn't match with initial data\n");
                    free(buf);
                    llCloseRx();
                    return;
                }

                char *name = NULL;
                int filesize = -1;
                while (index < size_buf)
                {
                    size_value = buf[index + 1];
                    if (buf[index] == 0)
                    {
                        filesize = 0;
                        for (int i = 0; i < size_value; i++)
                        {
                            filesize = (filesize << 8) | buf[index + 2 + i];
                        }
                    }
                    else if (buf[index] == 1)
                    {
                        name = malloc(size_value + 1);
                        memcpy(name, buf + index + 2, size_value);
                        name[size_value] = '\0';
                    }
                    else
                    {
                        printf("Invalid control packet\n");
                        free(buf);
                        llCloseRx();
                        return;
                    }
                    index += size_value + 2;
                }

                if (strcmp(file.filename, name) != 0 || file.filesize != filesize)
                {
                    printf("File name or size doesn't match with initial data\n");
                    free(buf);
                    llCloseRx();
                    return;
                }

                finished = 1;
                break;
            default:
                break;
            }
        }
        free(buf);
        // free(file.data)
        llCloseRx();
    }
    else
    {
        printf("Invalid role: %s. Must be 'tx' or 'rx'.\n", role);
        return;
    }
}
