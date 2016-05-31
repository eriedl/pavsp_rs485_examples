/* This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://www.wtfpl.net/ for more details.
 */

#include <Arduino.h>
#include <SoftwareSerial.h>

#define SSerialTx           8 // DI: data in
#define SSerialTxControl    9 // DE: data enable
#define RE_PIN              10 // RE: receive enable; jumpered together with 6, will be put into floating mode
#define SSerialRx           11 // RO: receive out
#define RS485Transmit       HIGH
#define RS485Receive        LOW

#define Pin13LED            13


/* Pump commands and instructions */
/*
   Every device on the bus has an address:
       0x0f - is the broadcast address, it is used by the more sophisticated controllers as <dst>
              in their system status broadcasts most likely to keep queries for system status low.
       0x1x - main controllers (IntelliComII, IntelliTouch, EasyTouch ...)
       0x2x - remote controllers
       0x6x - pumps, 0x60 is pump 1

   Let's use 0x20, the first address in the remote controller space
 */
#define PUMP_ADDRESS        0x60
#define MY_ADDRESS          0x11
#define BROADCAST_ADDRESS   0x0F

// Pump programming commands, this command writes instructions to a memory register. I.e. preset up to 4 programs to run
// Not sure how this relates to IFLOW_CMD_MODE...
// Are the following commands to run programs when the pump is already running and IFLOW_CMD_MODE is to set the mode before the pump is started?
#define IFLO_CMD_REG        0x01
#define IFLO_REG_EPRG       0x0321 // The register adress
#define IFLO_EPRG_OFF       0x0000 // Turn all external programs off
#define IFLO_EPRG_P1        0x0008 // Set External Program 1
#define IFLO_EPRG_P2        0x0010 // Set External Program 2
#define IFLO_EPRG_P3        0x0018 // Set External Program 3
#define IFLO_EPRG_P4        0x0020 // Set External Program 4

#define	IFLO_REG_EP1RPM	    0x0327	// Set speed for Ext. Prog. 1 ///* 4x160 */ <--- What does this mean???
#define	IFLO_REG_EP2RPM	    0x0328	// Set speed for Ext. Prog. 2
#define	IFLO_REG_EP3RPM	    0x0329	// Set speed for Ext. Prog. 3
#define	IFLO_REG_EP4RPM	    0x032a	// Set speed for Ext. Prog. 4

#define IFLO_CMD_CTRL       0x04
#define IFLO_CTRL_LOCAL     0x00
#define IFLO_CTRL_REMOTE    0xFF

// Not sure what the below does... According to Michael's readme in PABSHARE, one can set the also some modes with this. But it's not clear
// how these commands relate to IFLO_CMD_REG. Presumably the following just instructs the pump to run whatever has been programmed with IFLO_CMD_REG
// It also seems that these commands are sent once and the pump will just run with it, whereas IFLO_CMG_REG commands have to be repeated every 30 seconds
#define IFLO_CMD_MODE       0x05
#define	IFLO_MODE_FILTER	0x00 /* Filter */
#define	IFLO_MODE_MANUAL	0x01 /* Manual */
#define	IFLO_MODE_BKWASH	0x02
#define	IFLO_MODE______3	0x03 /* never seen */
#define	IFLO_MODE______4	0x04 /* never seen */
#define	IFLO_MODE______5	0x05 /* never seen */
#define	IFLO_MODE_FEATR1	0x06 /* Feature 1 */
#define	IFLO_MODE______7	0x07 /* never seen */
#define	IFLO_MODE______8	0x08 /* never seen */
#define	IFLO_MODE_EXT_P1	0x09 // Run Ext. Prog. 1
#define	IFLO_MODE_EXT_P2	0x0A // Run Ext. Prog. 2
#define	IFLO_MODE_EXT_P3	0x0B // Run Ext. Prog. 3
#define	IFLO_MODE_EXT_P4	0x0C // Run Ext. Prog. 4

#define IFLO_CMD_RUN        0x06
#define IFLO_RUN_STRT   	0x0A
#define IFLO_RUN_STOP   	0x04

#define IFLO_STATE_PRIMING  0x01
#define IFLO_STATE_RUNNING  0x02
#define IFLO_STATE_SYS_PRIMING  0x04

#define IFLO_CMD_STAT       0x07
/* EOF - Pump commands and instructions */

/* Pump message structure related stuff like indexes, constants, etc. */
#define PREAMBLE_LEN        0x04
const uint8_t PREAMBLE[] = { 0xFF, 0x00, 0xFF, 0xA5 };

// Pump message indexes
#define MSG_BGN_IDX         0x03
#define MSG_VER_IDX         0x04
#define MSG_DST_IDX         0x05
#define MSG_SRC_IDX         0x06
#define MSG_CFI_IDX         0x07
#define MSG_LEN_IDX         0x08

// Pump status data indexes
#define STAT_RUN_IDX        0x09
#define STAT_MODE_IDX       0x0A
#define STAT_STATE_IDX      0x0B
#define STAT_PWR_HB_IDX     0x0C
#define STAT_PWR_LB_IDX     0x0D
#define STAT_RPM_HB_IDX     0x0E
#define STAT_RPM_LB_IDX     0x0F
#define STAT_GPM_IDX        0x10
#define STAT_PPC_IDX        0x11 // Chlor level?
#define STAT_B09_IDX        0x12 // What's this?
#define STAT_ERR_IDX        0x13 // Whatever the error codes are. 0x00 is OK
#define STAT_B11_IDX        0x14 // What's this?
#define STAT_TIMER_IDX      0x15 // TIMER in minutes
#define STAT_CLK_HOUR_IDX   0x16
#define STAT_CLK_MIN_IDX    0x17

//                               P R E A M B L E      VER   DST           SRC         CFI               LEN   DAT                    CHB   CLB (Check Sum is set when sending command, depends on addresses used)
uint8_t setCtrlRemote[]    = {0xFF, 0x00, 0xFF, 0xA5, 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_CTRL,    0x01, IFLO_CTRL_REMOTE,      0x00, 0x00};
uint8_t setCtrlLocal[]     = {0xFF, 0x00, 0xFF, 0xA5, 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_CTRL,    0x01, IFLO_CTRL_LOCAL,       0x00, 0x00};
uint8_t getStatus[]        = {0xFF, 0x00, 0xFF, 0xA5, 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_STAT,    0x00,                        0x00, 0x00};
uint8_t extProgOff[]       = {0xFF, 0x00, 0xFF, 0xA5, 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_REG,     0x04, (IFLO_REG_EPRG >> 8), (IFLO_REG_EPRG & 0xFF), (IFLO_EPRG_OFF >> 8), (IFLO_EPRG_OFF & 0xFF), 0x00, 0x00};
uint8_t runExtProg1[]      = {0xFF, 0x00, 0xFF, 0xA5, 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_REG,     0x04, (IFLO_REG_EPRG >> 8), (IFLO_REG_EPRG & 0xFF), (IFLO_EPRG_P1 >> 8), (IFLO_EPRG_P1 & 0xFF), 0x00, 0x00};
uint8_t runExtProg2[]      = {0xFF, 0x00, 0xFF, 0xA5, 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_REG,     0x04, (IFLO_REG_EPRG >> 8), (IFLO_REG_EPRG & 0xFF), (IFLO_EPRG_P2 >> 8), (IFLO_EPRG_P2 & 0xFF), 0x00, 0x00};
uint8_t runExtProg3[]      = {0xFF, 0x00, 0xFF, 0xA5, 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_REG,     0x04, (IFLO_REG_EPRG >> 8), (IFLO_REG_EPRG & 0xFF), (IFLO_EPRG_P3 >> 8), (IFLO_EPRG_P3 & 0xFF), 0x00, 0x00};
uint8_t runExtProg4[]      = {0xFF, 0x00, 0xFF, 0xA5, 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_REG,     0x04, (IFLO_REG_EPRG >> 8), (IFLO_REG_EPRG & 0xFF), (IFLO_EPRG_P4 >> 8), (IFLO_EPRG_P4 & 0xFF), 0x00, 0x00};
uint8_t startPump[]        = {0xFF, 0x00, 0xFF, 0xA5, 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_RUN,     0x01, IFLO_RUN_STRT,         0x00, 0x00};
uint8_t stopPump[]         = {0xFF, 0x00, 0xFF, 0xA5, 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_RUN,     0x01, IFLO_RUN_STOP,         0x00, 0x00};
/* EOF - Pump message structure related stuff like indexes, constants, etc. */

SoftwareSerial RS485Serial(SSerialRx, SSerialTx); // RX, TX

/* Command input */
#define MAX_INPUT_LEN   16
uint8_t inputCmdBuffer[MAX_INPUT_LEN];
uint8_t *inputCmdBufPtr;
boolean inputComplete = false;
uint8_t *pumpCommand;
size_t pumpCommandSz;
uint8_t *lastCommand;
size_t lastCommandSz;
enum COMMAND_STAGE {
    IDLE,
    CTRL_REMOTE,
    CONFIRM_REMOTE,
    COMMAND,
    COMMAND_RESPONSE,
    CTRL_LOCAL,
    CONFIRM_LOCAL
};
enum COMMAND_STAGE curCmdStage;

/* RS485 message processing */
// The theoretical maximum is 264 bytes. however, it seems that actual message are much shorter, i.e. 37 bytes was
// the longest seen so far. So, allocate 2x size of an expected message size: 2 x 32 bytes.
//
//#define MAX_PKG_LEN         6+256+2 //Msg Prefix + Data + Checksum
#define MAX_PKG_LEN         6 + 32 + 2 // 40 bytes
#define MIN_PKG_LEN         6 + 0 + 2 // 8 bytes
#define RCV_BUF_SZ          (3 + MAX_PKG_LEN) * 2 // 86 bytes, includes preamble
uint8_t msgBuffer[RCV_BUF_SZ];
uint8_t *msgBufPtr;
#define RESPONSE_TTL        250
uint8_t responseTTL         = RESPONSE_TTL;
#define ISDEBUG             0

void setup() {
    Serial.begin(9600);
    Serial.println("RS485 Sender...");

    pinMode(SSerialTxControl, OUTPUT);

    pinMode(Pin13LED, OUTPUT);

    // Init Transceiver, we start with listening
    digitalWrite(SSerialTxControl, RS485Receive);

    // Start the software serial port, to another device
    RS485Serial.begin(9600);   // set the data rate

    reset(); // Reset all variables to their default
    digitalWrite(Pin13LED, LOW);   // turn LED off
}

void loop() {
    // Parse a command
    if (Serial.available()) {
        int iRcv = -1;

        while ((iRcv = Serial.read()) > -1) {
            // Line ending indicates end of command
            if (iRcv == 0x0A || iRcv == 0x0D) {
                inputComplete = true;
                break;
            } else {
                *inputCmdBufPtr++ = (char)iRcv;
            }

            // Reset the buffer
            if ((inputCmdBufPtr - inputCmdBuffer) > MAX_INPUT_LEN) { // Pointer arithmetic usually gives you the index, but we increment the pointer after every assignment, so we get now the length
                memset(inputCmdBuffer, 0, sizeof(inputCmdBuffer));
                inputCmdBufPtr = inputCmdBuffer;

                break;
            }
        }
    }

    // 1. Send Remote Ctrl
    // 1.1 Wait for confirmation
    // 2. Send Command
    // 2.1 Process confirmation/response, i.e. status
    // 3. Send Local Ctrl
    // 3.1 Wait for confirmation
    if (inputComplete == true) {
        String strCommand = String((char *) inputCmdBuffer);
        strCommand.toLowerCase();
        curCmdStage = CTRL_REMOTE;
        if (ISDEBUG) Serial.println("Command is: " + strCommand);

        if (strCommand.compareTo("on") == 0) {

        } else if (strCommand.compareTo("off") == 0) {

        } else if (strCommand.compareTo("status") == 0) {
            Serial.println("Querying pump status");
            pumpCommand = getStatus;
            pumpCommandSz = sizeof(getStatus);
        } else if (strCommand.compareTo("reset") == 0) {
            Serial.println("Resetting system");
            reset();
        } else if (strCommand.compareTo("control remote") == 0) {
            Serial.println("Setting control remote");
            pumpCommandSz = 0;
            curCmdStage = CTRL_REMOTE;
        } else if (strCommand.compareTo("control local") == 0) {
            Serial.println("Setting control local");
            pumpCommandSz = 0;
            curCmdStage = CTRL_LOCAL;
        } else {
            Serial.print("What? Resetting....");
            reset();
        }

        inputComplete = false;
        memset(inputCmdBuffer, 0, sizeof(inputCmdBuffer));
        inputCmdBufPtr = inputCmdBuffer;
    }

    // 1. Send Remote Ctrl
    if (curCmdStage == CTRL_REMOTE || curCmdStage == COMMAND || curCmdStage == CTRL_LOCAL) {
        // Compose the message
        uint16_t chkSum = 0;
        switch(curCmdStage) {
            case CTRL_REMOTE:
                Serial.println("Setting command stage to CONFIRM_REMOTE");
                lastCommandSz = sizeof(setCtrlRemote);
                lastCommand = setCtrlRemote;
                curCmdStage = CONFIRM_REMOTE;
                break;
            case COMMAND:
                Serial.println("Setting command stage to COMMAND_RESPONSE");
                lastCommandSz = pumpCommandSz;
                lastCommand = pumpCommand;
                curCmdStage = COMMAND_RESPONSE;
                break;
            case CTRL_LOCAL:
                Serial.println("Setting command stage to CONFIRM_LOCAL");
                lastCommandSz = sizeof(setCtrlLocal);
                lastCommand = setCtrlLocal;
                curCmdStage = CONFIRM_LOCAL;
                break;
            default:
                reset();
                return;
        }

        digitalWrite(SSerialTxControl, RS485Transmit);

        if (ISDEBUG) Serial.print("Sending data: ");
        for(int i = 0; i < lastCommandSz; ++i) {
            uint8_t b = lastCommand[i];

            //Calculate the checksum
            if (i >= MSG_BGN_IDX && i < (lastCommandSz - 2)) {
                chkSum += b;
            } else if (i == (lastCommandSz - 2)) {
                b = (uint8_t)(chkSum >> 8);
            } else if (i == (lastCommandSz - 1)) {
                b = (uint8_t)(chkSum & 0xFF);
            }

            if (ISDEBUG) Serial.print(b < 0x10 ? "0" + String(b, HEX) : String(b, HEX));
            RS485Serial.write(b);
        }
        if (ISDEBUG) Serial.println(", Checksum: " + String(chkSum));

        digitalWrite(SSerialTxControl, RS485Receive);
    }

    // Receive data from remote
    if ((curCmdStage == CONFIRM_REMOTE || curCmdStage == COMMAND_RESPONSE || curCmdStage == CONFIRM_LOCAL)) {

        if (RS485Serial.available() > 0) {
            int iRcv = -1;

            if (ISDEBUG) Serial.print("Incoming data: ");
            while ((iRcv = RS485Serial.read()) > -1) {
                *msgBufPtr++ = (char)iRcv;

                if (ISDEBUG) Serial.print(iRcv < 0x10 ? "0" + String(iRcv, HEX) : String(iRcv, HEX));

                // Reset the buffer, if we overflow. That should not happen
                if ((msgBufPtr - msgBuffer) > RCV_BUF_SZ) {
                    Serial.println("Buffer overflow. Resetting system...");
                    reset();

                    return;
                }
            }
            if (ISDEBUG) Serial.println();

            // Now check if we got a useful message
            int bufLen = (msgBufPtr - msgBuffer);
            if (ISDEBUG) Serial.println("len: " + String(bufLen) + ", min len: " + String((PREAMBLE_LEN - 1 + MIN_PKG_LEN)));
            if (bufLen >= (PREAMBLE_LEN - 1 + MIN_PKG_LEN)) {
                int msgStartIdx = -1;
                int msgLength = -1;
                // FIXME: Handle multiple consecutive messages in buffer. Ideally make this a loop until no message
                boolean msgFound = findPAMessage(msgBuffer, bufLen, &msgStartIdx, &msgLength);
                if (ISDEBUG) Serial.println("Message found: " + String(msgFound));

                if (msgFound == true) {
                    uint8_t *relMsgPtr = &msgBuffer[msgStartIdx];
                    // Reset the buffer and bail if the message was not for us
                    if (relMsgPtr[MSG_DST_IDX] != MY_ADDRESS || relMsgPtr[MSG_SRC_IDX] != PUMP_ADDRESS) {
                        Serial.println("Message not for us:"
                                               " SRC: " + String(relMsgPtr[MSG_DST_IDX], HEX) +
                                               " DST: " + String(relMsgPtr[MSG_SRC_IDX], HEX));
                        memset(msgBuffer, 0, sizeof(msgBuffer));
                        msgBufPtr = msgBuffer;
                        responseTTL = RESPONSE_TTL;

                        return;
                    }

                    // Response is not for our last command
                    if (relMsgPtr[MSG_CFI_IDX] != lastCommand[MSG_CFI_IDX]) {
                        Serial.println("Message is not a confirmation of our last command:"
                                               " SENT: " + String(lastCommand[MSG_CFI_IDX], HEX) +
                                               " RCVD: " + String(relMsgPtr[MSG_CFI_IDX], HEX));
                        reset();

                        return;
                    }

                    // Response should also contain the last command value, unless it was a status request
                    uint8_t dataLen = relMsgPtr[MSG_LEN_IDX];
                    if (lastCommand[MSG_CFI_IDX] == IFLO_CMD_STAT && dataLen == 15) {
                        Serial.println("Pump status: ");
                        Serial.println("\tRUN: " + String(relMsgPtr[STAT_RUN_IDX]));
                        Serial.println("\tMOD: " + String(relMsgPtr[STAT_MODE_IDX]));
                        Serial.println("\tPMP: " + String(relMsgPtr[STAT_STATE_IDX]));
                        Serial.println("\tPWR: " + String((relMsgPtr[STAT_PWR_HB_IDX] * 256) + relMsgPtr[STAT_PWR_LB_IDX]) + " WATT");
                        Serial.println("\tRPM: " + String((relMsgPtr[STAT_RPM_HB_IDX] * 256) + relMsgPtr[STAT_RPM_LB_IDX]) + " RPM");
                        Serial.println("\tGPM: " + String(relMsgPtr[STAT_GPM_IDX]) + " GPM");
                        Serial.println("\tPPC: " + String(relMsgPtr[STAT_PPC_IDX]) + " %");
                        Serial.println("\tB09: " + String(relMsgPtr[STAT_B09_IDX]));
                        Serial.println("\tERR: " + String(relMsgPtr[STAT_ERR_IDX]));
                        Serial.println("\tB11: " + String(relMsgPtr[STAT_B11_IDX]));
                        Serial.println("\tTMR: " + String(relMsgPtr[STAT_TIMER_IDX]) + " MIN");
                        Serial.println("\tCLK: " + String(relMsgPtr[STAT_CLK_HOUR_IDX]) + ":" + (relMsgPtr[STAT_CLK_MIN_IDX] < 0x0A ? "0" + String(relMsgPtr[STAT_CLK_MIN_IDX]) : relMsgPtr[STAT_CLK_MIN_IDX]));
                    } else {
                        for (int i = MSG_LEN_IDX + 1; i < dataLen; ++i) {
                            uint8_t lCmdVal = 0x00;

                            // Register writes echo only the value!
                            if (lastCommand[MSG_CFI_IDX] == IFLO_CMD_REG) {
                                lCmdVal = lastCommand[i + sizeof(IFLO_REG_EPRG)];
                            } else {
                                lCmdVal = lastCommand[i];
                            }

                            if (relMsgPtr[i] != lCmdVal) {
                                Serial.println(
                                        "Message does not contain expected value of sent command value at data index " +
                                        String(i) +
                                        " SENT: " + String(lCmdVal) +
                                        " RCVD: " + String(relMsgPtr[i]));

                                reset();

                                return;
                            }
                        }
                    }

                    if (ISDEBUG) Serial.println("Last command confirmed");

                    if (pumpCommandSz == 0) {
                        Serial.println("Control mode has been reset");
                        reset();

                        return;
                    }

                    // We should be good now
                    switch(curCmdStage) {
                        case CONFIRM_REMOTE:
                            Serial.println("Setting command stage to COMMAND");
                            curCmdStage = COMMAND;
                            break;
                        case COMMAND_RESPONSE:
                            Serial.println("Setting command stage to CTRL_LOCAL");
                            curCmdStage = CTRL_LOCAL;
                            break;
                        case CONFIRM_LOCAL:
                            Serial.println("Setting command stage to IDLE");
                            reset();
                            break;
                    }

                    // Reset the receiving buffer
                    memset(msgBuffer, 0, sizeof(msgBuffer));
                    msgBufPtr = msgBuffer;
                    responseTTL = RESPONSE_TTL;
                }
//                else {
//                    // We found the start of a message but either the message was incomplete or the checksum failed. Reset everything;
//                    if (msgStartIdx > -1 || msgLength == 0) {
//                        if (responseTTL > 0) {
//                            Serial.println("Partial message received. Waiting for completion...");
//                        } else {
//                            Serial.println("Partial message received. Did not receive missing parts, resetting...");
//                            reset();
//                        }
//                    }
//                }
            }
        } else {
//            // Waiting for message completion
//            if (responseTTL > 0) {
//                --responseTTL;
//                Serial.println("TTL: " + String(responseTTL));
//            } else {
//                Serial.println("Partial message received. Did not receive missing parts, resetting...");
//                reset();
//            }
        }
    }
}

void reset() {
    memset(inputCmdBuffer, 0, sizeof(inputCmdBuffer));
    inputCmdBufPtr = inputCmdBuffer;
    inputComplete = false;
    memset(msgBuffer, 0, sizeof(msgBuffer));
    msgBufPtr = msgBuffer;
    pumpCommandSz = 0;
    lastCommandSz = 0;
    curCmdStage = IDLE;
    responseTTL = RESPONSE_TTL;
}

boolean findPAMessage(uint8_t data[], int len, int *msgStartIdx, int *actualMsgLength) {
    *msgStartIdx = -1; // the starting index of a valid message, if any. 0xA5
    *actualMsgLength = 0;

    // Find the start of the message
    for (int i = 0; i < (len - PREAMBLE_LEN); ++i) {
        if (data[i] == PREAMBLE[0]
            && data[i + 1] == PREAMBLE[1]
            && data[i + 2] == PREAMBLE[2]
            && data[i + 3] == PREAMBLE[3]) {
            *msgStartIdx = i + 3;
            break;
        }
    }

    if (ISDEBUG) Serial.println("msgStartIdx: " + String(*msgStartIdx));
    // Check if the found start index + minimum package length is within total length of message
    if (*msgStartIdx == -1 || (*msgStartIdx + MIN_PKG_LEN) > len) {
        return false;
    }

    // Get the actual data length. It's the eigth byte in the message including preamble (or 5th starting at MSG_BGN_IDX
    int dataLength = data[*msgStartIdx + (MSG_LEN_IDX - MSG_BGN_IDX)];
    if (ISDEBUG) Serial.println("dataLength: " + String(dataLength));
    // Check again if total message length is within the boundaries given the data length
    *actualMsgLength = (MSG_LEN_IDX - MSG_BGN_IDX) + 1 + dataLength + 2; // Index + 1, actual data length, check sum length
    if (*msgStartIdx + *actualMsgLength > len) {
        return false;
    }

    if (ISDEBUG) Serial.println("actualMsgLength: " + String(*actualMsgLength));
    // Calculate the checksum
    int chkSum = 0;
    int expChkSum = (data[*msgStartIdx + *actualMsgLength - 2] * 256) + data[*msgStartIdx + *actualMsgLength - 1];
    for (int i = *msgStartIdx; i < (*msgStartIdx + *actualMsgLength - 2); ++i) {
        chkSum += data[i];
    }

    // Add the leading garbage again to the message
    *msgStartIdx -= (PREAMBLE_LEN - 1);
    *actualMsgLength += (PREAMBLE_LEN - 1);

    if (ISDEBUG) Serial.println("msgStartIdx: " + String(*msgStartIdx));
    if (ISDEBUG) Serial.println("actualMsgLength: " + String(*actualMsgLength));
    if (ISDEBUG) Serial.println("Calc checksum: " + String(chkSum));
    if (ISDEBUG) Serial.println("High byte: " + String(data[*msgStartIdx + *actualMsgLength - 2]));
    if (ISDEBUG) Serial.println("Low byte: " + String(data[*msgStartIdx + *actualMsgLength - 1]));
    if (ISDEBUG) Serial.println("Exp checksum: " + String(expChkSum));

    // Verify the checksum
    if (chkSum != expChkSum) {
        if (ISDEBUG) Serial.println("Checksum is: BAD");

        return false;
    }

    if (ISDEBUG) Serial.println("Checksum is: GOOD");
    return true;
}