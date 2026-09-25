// RCOM 2026/2027
//
// Serial port header.
// DO NOT CHANGE THIS FILE

#ifndef SERIAL_PORT_H
#define SERIAL_PORT_H

/**
 * Open and configure the serial port.
 *
 * @param serialPort: Name of the serial port (e.g., "/dev/ttyS0").
 * @param baudRate: Baud rate for the serial port.
 * @return a positive number if the port was opened successfully or -1 on error.
 */
int openSerialPort(const char *serialPort, int baudRate);

/**
 * Restore original port settings and close the serial port.
 *
 * @return 0 if the port was closed successfully or -1 on error.
 */
int closeSerialPort();

/**
 * Wait up to 0.1 second (VTIME) for a byte received from the serial port (must
 * check whether a byte was actually received from the return value).
 *
 * @param byte: Pointer to the byte which will contain the byte read.
 * @return -1 on error, 0 if no byte was received, 1 if a byte was received.
 */
int readByteSerialPort(unsigned char *byte);

/**
 * Write up to numBytes to the serial port (must check how many were actually
 * written in the return value).
 *
 * @param bytes: Pointer to the buffer of bytes to write.
 * @param nBytes: Number of bytes to write.
 * @return -1 on error, otherwise the number of bytes written.
 */
int writeBytesSerialPort(const unsigned char *bytes, int nBytes);

#endif // SERIAL_PORT_H
