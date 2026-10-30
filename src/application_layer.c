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
        
        FILE *file = fopen(filename, "rb");
        if (!file) {
            perror("fopen");
            return;
        }

        int fileSize;
        fseek(file, 0, SEEK_END);
        fileSize = ftell(file);
        if (fileSize < 0) {
            perror("ftell");
            fclose(file);
            return;
        }
        fseek(file, 0, SEEK_SET);

        if (llOpenTx(llParameters) < 0){
            fclose(file);
            return;
        } 
        
        // Start Control packet
        unsigned char controlPacket[1 + 2 + 4 + 2 + strlen(filename)];
        controlPacket[0] = 1;     
        controlPacket[1] = 0; 
        controlPacket[2] = 4; 
        controlPacket[3] = (fileSize >> 24) & 0xFF;
        controlPacket[4] = (fileSize >> 16) & 0xFF;
        controlPacket[5] = (fileSize >> 8) & 0xFF;
        controlPacket[6] = fileSize & 0xFF;
        controlPacket[7] = 1; 
        controlPacket[8] = strlen(filename); 
        memcpy(controlPacket + 9, filename, strlen(filename));

        if (llSend(controlPacket, sizeof(controlPacket)) < 0) {
            perror("llSend");
            fclose(file);
            llCloseTx();
            return;
        }
        
        // Data packets
        unsigned char packet[MAX_PAYLOAD_SIZE];
        int bytesRead = 0;
        int atualSize = fileSize;
        while (atualSize > 0) {
            bytesRead = fread(packet+3, 1, MAX_PAYLOAD_SIZE - 3, file);
            if (bytesRead <= 0) {
                perror("fread");
                fclose(file); 
                llCloseTx(); //because of this, need to implement edge case in the receiver in llreceive, to avoid the receiver to wait for a packet that will never arrive
                return;
            }
            atualSize -= bytesRead;
            packet[0] = 2;
            packet[1] = (bytesRead) / 256;
            packet[2] = (bytesRead) % 256;
            if (llSend((const unsigned char *)packet, bytesRead + 3) < 0) {
                perror("llSend");
                fclose(file);
                llCloseTx(); //because of this, need to implement edge case in the receiver in llreceive, to avoid the receiver to wait for a packet that will never arrive
                return;
            }
        }

        // End control packet
        controlPacket[0] = 3; // Control packet type
        if (llSend(controlPacket, sizeof(controlPacket)) < 0) {
            perror("llSend");
            fclose(file);
            llCloseTx();
            return;
        }
        
        fclose(file);
        llCloseTx();
    }
    else if (strcmp(role, "rx") == 0){

        if (llOpenRx(llParameters) < 0) return;

        FILE *newFile = NULL;
        char *rxFilename = NULL;
        int rxFilesize = -1;

        unsigned char buf[MAX_PAYLOAD_SIZE];
        int size_atual=0, size_value=0, acc=0;
        const char* fileName = NULL;
        
        unsigned char finished = 0;
        
        while (!finished){
            int size_buf = llReceive(buf);
            if (size_buf == -2){
                printf("Connection closed by transmitter\n");
                if (newFile) fclose(newFile);
                if (rxFilename) free(rxFilename);
                return;
            }
            if (size_buf < 0) {
                perror("llReceive");
                if (newFile) fclose(newFile);
                if (rxFilename) free(rxFilename);
                llCloseRx();
                return;
            }
            if (size_buf == 0) continue;
            
            int index=1;
            switch (buf[0]){
                case 1:
                    while (index<size_buf){
                        size_value = buf[index+1];
                        if (buf[index]==0){ // File size
                            rxFilesize = 0;
                            for (int i=0; i<size_value; i++){
                                rxFilesize = (rxFilesize << 8) | buf[index + 2 + i]; 
                            }
                        }
                        else if (buf[index]==1){ // File name
                            rxFilename = malloc(size_value + 1);
                            memcpy(rxFilename, buf + index + 2, size_value);
                            rxFilename[size_value] = '\0';
                        }
                        else{
                            printf("Invalid control packet\n");
                            if (rxFilename) free(rxFilename);
                            if (newFile) fclose(newFile);
                            llCloseRx();
                            return;
                        }
                        index+=size_value+2;
                    }

                    if (rxFilesize < 0){
                        printf("Invalid file size / File size not indicated\n");
                        if (rxFilename) free(rxFilename);
                        if (newFile) fclose(newFile);
                        llCloseRx();
                        return;
                    }

                    const char *outputName = NULL;
                    if (filename != NULL && strlen(filename) > 0) {
                        outputName = filename;
                    } else if (rxFilename != NULL && strlen(rxFilename) > 0) {
                        outputName = rxFilename;
                    } else {
                        outputName = "received_file";
                    }

                    fileName = outputName;
                    newFile = fopen(fileName, "wb");
                    if (!newFile) {
                        perror("fopen");
                        if (rxFilename) free(rxFilename);
                        llCloseRx();
                        return;
                    }
                    break;
                case 2:
                    if (rxFilesize==-1){ 
                        printf("The file size must be indicated before sending data\n");
                        if (rxFilename) free(rxFilename);
                        if (newFile) fclose(newFile);
                        llCloseRx();
                        return;
                    }
                    size_atual = 256*buf[1]+buf[2];
                    acc+=size_atual;
                    if (acc>rxFilesize){
                        printf("The file data is bigger than the file size initial planned\n");
                        if (rxFilename) free(rxFilename);
                        if (newFile) fclose(newFile);
                        llCloseRx();
                        return;
                    }
                    fwrite(buf+3, 1, size_atual, newFile);
                    break;
                case 3:
                    
                    if (rxFilesize==-1){ 
                        printf("Invalid order of control packet\n");
                        if (rxFilename) free(rxFilename);
                        if (newFile) fclose(newFile);
                        llCloseRx();
                        return;
                    }
                
                    char* name = NULL;
                    int filesize = -1;
                    
                    while (index<size_buf){
                        size_value = buf[index+1];
                        if (buf[index]==0){ // File size
                            filesize = 0;
                            for (int i=0; i<size_value; i++){
                                filesize = (filesize << 8) | buf[index + 2 + i];
                            }
                        }
                        else if (buf[index]==1){  // File name
                            name = malloc(size_value + 1);
                            memcpy(name, buf+index+2, size_value);
                            name[size_value] = '\0';
                        }
                        else{
                            printf("Invalid control packet\n");
                            if (rxFilename) free(rxFilename);
                            if (newFile) fclose(newFile);
                            llCloseRx();
                            return;
                        }
                        index+=size_value+2;
                    }                                     
                    
                    if (filesize != rxFilesize || acc != rxFilesize) {
                        fprintf(stderr, "Warning: File size doesn't match with received data!\n");
                    }
                    if (rxFilename && name && strcmp(rxFilename, name) != 0) {
                        fprintf(stderr, "Warning: File name doesn't match with initial data!\n");
                    }

                    if (name) free(name);
                    if (rxFilename) free(rxFilename);
                    if (newFile) fclose(newFile);
                    
                    finished = 1;
                    printf("File %s received successfully!\n", fileName);
                    break;
                default:
                    break;    
            }
        }
        llCloseRx();
    }
    else{
        printf("Invalid role: %s. Must be 'tx' or 'rx'.\n", role);
        return;
    }
}
