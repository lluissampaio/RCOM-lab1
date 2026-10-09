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

        // TODO fazer o Control Packet
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

        // TODO Avisar que já acabou com Control Packet

        llCloseTx();
    }
    else if (strcmp(role, "rx") == 0){

        if (llOpenRx(llParameters) < 0) return;

        // TODO, pensar se acrescento parametros
        // TODO, testar

        typedef struct{
            char* filename;//Opcional mandar
            int filesize;//Obrigatorio mandar  
            unsigned char* data;       
        } File;
        
        File file = {NULL,-1,NULL};

        unsigned char* buf = malloc(MAX_PAYLOAD_SIZE);
        if (buf == NULL) {
            llCloseRx();
            return;
        }
        int size_atual=0, size_value=0, size_buf=0, acc=0;
        int index=1;
        unsigned char finished = 0;
        while (!finished){
            size_buf = llReceive(buf);
            if (size_buf < 0) {
                perror("llReceive");
                free(buf);
                llCloseRx();
                return;
            }
            if (size_buf == 0) continue;
            if (file.data == NULL && file.filesize >= 0){
                file.data=malloc(file.filesize);
                if (file.data == NULL){
                    printf("Failing doing mem allocation to File Data\n");
                    free(buf);
                    llCloseRx();
                    return;
                }
            } 
            index=1;
            switch (buf[0]){
                case 1:
                    while (index<size_buf){
                        size_value = buf[index+1];
                        if (buf[index]==0){
                            file.filesize=0;
                            for (int i=0; i<size_value; i++){
                                file.filesize += buf[index+2+i]*pow(256, size_value-i-1); //IA generated this line
                            }
                        }
                        else if (buf[index]==1){ 
                            file.filename = malloc(size_value);
                            memcpy(file.filename, buf+index+2, size_value);
                        }
                        else{
                            printf("Invalid control packet\n");
                            free(buf);
                            llCloseRx();
                            return;
                        }
                        index+=size_value+2;
                    }                  
                    break;
                case 2:
                    if (file.filesize==-1){ 
                        printf("The file size must be indicated before sending data\n");
                        free(buf);
                        llCloseRx();
                        return;
                    }
                    size_atual = 256*buf[1]+buf[2];
                    if (acc+size_atual>file.filesize){
                        printf("The file data is bigger than the file size initial planned\n");
                        free(buf);
                        llCloseRx();
                        return;
                    }
                    memcpy(file.data+acc,buf+3,size_atual); 
                    acc+=size_atual;
                    break;
                case 3:
                    if (file.filesize != acc){
                        printf("File data doesn't match with initial data\n");
                        free(buf);
                        llCloseRx();
                        return;
                    }
                    
                    char* name = NULL;
                    int filesize = -1;
                    while (index<size_buf){
                        size_value = buf[index+1];
                        if (buf[index]==0){
                            filesize = 0;
                            for (int i=0; i<size_value; i++){
                                filesize += buf[index+2+i]*pow(256, size_value-i-1); //IA generated this line
                            }
                        }
                        else if (buf[index]==1){ 
                            name = malloc(size_value);
                            memcpy(name, buf+index+2, size_value);
                        }
                        else{
                            printf("Invalid control packet\n");
                            free(buf);
                            llCloseRx();
                            return;
                        }
                        index+=size_value+2;
                    }                                     
                    break;
                    
                    if (file.filename != name || file.filesize != filesize){
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
        //free(file.data)
        llCloseRx();
    }
    else{
        printf("Invalid role: %s. Must be 'tx' or 'rx'.\n", role);
        return;
    }
}
