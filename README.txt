INSTRUCTIONS FOR SERIAL PORT PROTOCOL
=====================================

This folder contains the base code of the serial port protocol.

Project Structure
-----------------

- bin/: Compiled binaries.
- src/: Source code for the implementation of the link-layer and application layer protocols. Students should edit these files to implement the project.
- cable/: Virtual cable program to help test the serial port. This file must not be changed.
- Makefile: Makefile to build the project and run the application.
- penguin.gif: Example file to be sent through the serial port.

Instructions to Run the Project
-------------------------------

1. Edit the source code in the src/ directory.
2. Compile the application and the virtual cable program using the provided Makefile.
3. Run the virtual cable program (either by running the executable manually or using the Makefile target).
   Note that the virtual cable program requires the installation of "socat".
    (Option 1) $ ./bin/cable_app
    (Option 2) $ make run_cable

4. Test the protocol without cable disconnections and noise
    4.1 Run the receiver (either by running the executable manually or using the Makefile target):
        (Option 1) $ ./bin/main /tmp/ttyS11 9600 rx penguin-received.gif
        (Option 2) $ make run_rx

    4.2 Run the transmitter (either by running the executable manually or using the Makefile target):
        (Option 1) $ ./bin/main /tmp/ttyS10 9600 tx penguin.gif
        (Option 2) $ make run_tx

    4.3 Check if the file received matches the file sent, using the diff Linux command or using the Makefile target:
        (Option 1) $ diff -s penguin.gif penguin-received.gif
        (Option 2) $ make check_files

5. Test the protocol with cable disconnections and noise
    5.1. Run receiver and transmitter again
    5.2. Quickly move to the cable program console and disable the connection and/or add noise. Check the instructions printed at the start of the program.
    5.3. Check if the file received matches the file sent, even with cable disconnections or with noise

Additional Protocol Details & Edge Cases Implemented
----------------------------------------------------

1. Lost UA Frame During Connection Establishment (SET inside llReceive):
   If the UA frame sent by the receiver in llOpenRx is lost or corrupted, the receiver moves on to llReceive while the transmitter times out in llOpenTx and retransmits SET. To prevent a deadlock, llReceive accepts SET control frames, resends the UA response, and returns to the START state to await Information frames.

2. Early Connection Termination by Transmitter (DISC inside llReceive):
   If the transmitter aborts mid-transfer (e.g., after exhausting retransmissions on a data packet or encountering a file read error) and calls llCloseTx(), the receiver would otherwise remain blocked in llReceive waiting for data packets. To handle this gracefully, llReceive detects DISC frames, replies with DISC, waits for the final UA, closes the serial port, and returns -2 so the application layer can clean up resources and exit cleanly.

3. Corrupted Closing FLAG in Information Frames (atual > MAX_PAYLOAD_SIZE + 1):
   If noise corrupts the closing FLAG of an I frame, the receiver would remain in the DATA state and mistakenly consume the opening FLAG of the next retransmitted frame as the closing FLAG of the corrupted one. By checking if the number of unstuffed bytes read in DATA exceeds MAX_PAYLOAD_SIZE + 1 (payload + BCC2), llReceive immediately replies with REJ (or RR if duplicate) and resets the state machine to START before the next frame arrives.

