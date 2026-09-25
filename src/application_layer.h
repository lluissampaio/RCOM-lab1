// RCOM 2026/2027
//
// Application layer protocol header.
// DO NOT CHANGE THIS FILE

#ifndef APPLICATION_LAYER_H
#define APPLICATION_LAYER_H

/**
 * Application layer main function.
 *
 * @param serialPort: Serial port name (e.g., /dev/ttyS0 or /tmp/ttyS10).
 * @param role: Application role {"tx", "rx"}.
 * @param baudrate: Baudrate of the serial port.
 * @param nTries: Maximum number of frame retries.
 * @param timeout: Frame timeout.
 * @param filename: Name of the file to send / receive.
 * @return 0 on success or -1 on error.
 */
void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename);

#endif // APPLICATION_LAYER_H
