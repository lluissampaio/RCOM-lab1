// RCOM 2026/2027
//
// Link layer header.
// DO NOT CHANGE THIS FILE

#ifndef LINK_LAYER_H
#define LINK_LAYER_H

typedef struct
{
    char serialPort[50];  // Serial port device (e.g., "/dev/ttyS0" or "/tmp/ttyS10")
    int baudRate;         // Speed of the transmission
    int nRetransmissions; // Number of retries in case of failure
    int timeout;          // Retransmission timeout value in seconds
} LinkLayer;

// Size of maximum acceptable payload.
// Maximum number of bytes that application layer should send to link layer.
#define MAX_PAYLOAD_SIZE 1000

// MISC
#define FALSE 0
#define TRUE 1

/**
 * Open the link layer connection as a transmitter (Tx) using the parameters
 * passed as argument.
 *
 * @param llParameters The link layer parameters.
 * @return 0 on success or -1 on error.
 */
int llOpenTx(LinkLayer llParameters);

/**
 * Open the link layer connection as a receiver (Rx) using the parameters
 * passed as argument.
 *
 * @param llParameters The link layer parameters.
 * @return 0 on success or -1 on error.
 */
int llOpenRx(LinkLayer llParameters);

/**
 * Send data in buf with size bufSize.
 *
 * @param buf The buffer containing the data to send.
 * @param bufSize The size of the buffer.
 * @return number of chars written, or -1 on error.
 */
int llSend(const unsigned char *buf, int bufSize);

/**
 * Receive data in packet.
 *
 * @param packet The buffer to store the received data.
 * @return number of chars read, or -1 on error.
 */
int llReceive(unsigned char *packet);

/**
 * Close the previously opened connection as a transmitter (Tx) and
 * print the transmission statistics in the console.
 *
 * @return 0 on success or -1 on error.
 */
int llCloseTx();

/**
 * Close the previously opened connection as a receiver (Rx) and
 * print the transmission statistics in the console.
 *
 * @return 0 on success or -1 on error.
 */
int llCloseRx();

#endif // LINK_LAYER_H
