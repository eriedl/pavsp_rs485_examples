/* This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://www.wtfpl.net/ for more details.
 */

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <Timer.h>

#define SSerialTx           8 // DI: data in
#define SSerialTxControl    9 // DE: data enable
#define RE_PIN              10 // RE: receive enable; jumpered together with 6, will be put into floating mode
#define SSerialRx           11 // RO: receive out
#define RS485Transmit       HIGH
#define RS485Receive        LOW

#define Pin13LED            13

/* Pump commands and instructions */
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

// Is supposedly reorted on byte 3, but it's alwasy 0x00 in my case. It may depend on the system setup or pump model
#define IFLO_STATE_PRIMING  0x01
#define IFLO_STATE_RUNNING  0x02
#define IFLO_STATE_SYS_PRIMING  0x04

#define IFLO_CMD_STAT       0x07
/* EOF - Pump commands and instructions */

/* Pump message structure related stuff like indexes, constants, etc. */
/*
   Every device on the bus has an address:
       0x0f - is the broadcast address, it is used by the more sophisticated controllers as <dst>
              in their system status broadcasts most likely to keep queries for system status low.
       0x1x - main controllers (IntelliComII, IntelliTouch, EasyTouch ...)
       0x2x - remote controllers
       0x6x - pumps, 0x60 is pump 1

   Let's use 0x20, the first address in the remote controller space
 */
uint8_t PUMP_ADDRESS        = 0x60;
uint8_t MY_ADDRESS          = 0x20;

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

//                                             P R E A M B L E                     VER   DST           SRC         CFI               LEN   DAT                    CHB   CLB (Check Sum is set when sending command, depends on addresses used)
uint8_t setCtrlRemote[]     = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_CTRL,    0x01, IFLO_CTRL_REMOTE,      0x00, 0x00};
uint8_t setCtrlLocal[]      = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_CTRL,    0x01, IFLO_CTRL_LOCAL,       0x00, 0x00};
uint8_t getStatus[]         = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_STAT,    0x00,                        0x00, 0x00};
uint8_t extProgOff[]        = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_REG,     0x04, (IFLO_REG_EPRG >> 8), (IFLO_REG_EPRG & 0xFF), (IFLO_EPRG_OFF >> 8), (IFLO_EPRG_OFF & 0xFF), 0x00, 0x00};
uint8_t runExtProg1[]       = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_REG,     0x04, (IFLO_REG_EPRG >> 8), (IFLO_REG_EPRG & 0xFF), (IFLO_EPRG_P1 >> 8), (IFLO_EPRG_P1 & 0xFF), 0x00, 0x00};
uint8_t runExtProg2[]       = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_REG,     0x04, (IFLO_REG_EPRG >> 8), (IFLO_REG_EPRG & 0xFF), (IFLO_EPRG_P2 >> 8), (IFLO_EPRG_P2 & 0xFF), 0x00, 0x00};
uint8_t runExtProg3[]       = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_REG,     0x04, (IFLO_REG_EPRG >> 8), (IFLO_REG_EPRG & 0xFF), (IFLO_EPRG_P3 >> 8), (IFLO_EPRG_P3 & 0xFF), 0x00, 0x00};
uint8_t runExtProg4[]       = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_REG,     0x04, (IFLO_REG_EPRG >> 8), (IFLO_REG_EPRG & 0xFF), (IFLO_EPRG_P4 >> 8), (IFLO_EPRG_P4 & 0xFF), 0x00, 0x00};
uint8_t startPump[]         = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_RUN,     0x01, IFLO_RUN_STRT,         0x00, 0x00};
uint8_t stopPump[]          = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_RUN,     0x01, IFLO_RUN_STOP,         0x00, 0x00};
/* EOF - Pump message structure related stuff like indexes, constants, etc. */

SoftwareSerial RS485Serial(SSerialRx, SSerialTx); // RX, TX

/* Serial command input */
#define MAX_INPUT_LEN       16
uint8_t inputCmdBuffer[MAX_INPUT_LEN];
uint8_t *inputCmdBufPtr;
/* EOF - Serial command input */

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
/* EOF - RS485 message processing */

/* State machine */
// 1. Send Remote Ctrl
// 1.1 Wait for confirmation
// 2. Send Command
// 2.1 Process confirmation/response, i.e. status
// 3. Send Local Ctrl
// 3.1 Wait for confirmation
enum COMMAND_STAGE {
    IDLE,
    CTRL_REMOTE,
    CONFIRM_REMOTE,
    COMMAND,
    COMMAND_RESPONSE,
    CTRL_LOCAL,
    CONFIRM_LOCAL,
    UPDATE_ATTRIBUTES
};
enum COMMAND {
    CMD_NOOP,
    CMD_ALL_OFF,
    CMD_RUN_PROG_1,
    CMD_RUN_PROG_2,
    CMD_RUN_PROG_3,
    CMD_RUN_PROG_4,
    CMD_STATUS,
    CMD_CTRL_REMOTE,
    CMD_CTRL_LOCAL,
    CMD_PUMP_ON,
    CMD_PUMP_OFF
};

enum COMMAND_STAGE curCmdStage;
uint8_t curCommand;
uint8_t *pumpCommand;
size_t pumpCommandSz;
uint8_t *lastCommand;
size_t lastCommandSz;

struct pump_status {
    uint8_t running;
    uint8_t mode;
    uint8_t drive_state;
    uint16_t pwr_usage;
    uint16_t speed;
    uint8_t flow_rate;
    uint8_t ppc_levels;
    uint8_t b09;
    uint8_t error_code;
    uint8_t b11;
    uint8_t timer;
    char clock[6] = "--:--";
};

pump_status pumpStatus;

#define STATUS_QRY_TIMEOUT  1000 * 15   // 15 seconds
#define COMMAND_TIMEOUT     1000 * 2    // 2 second
Timer timer1;
int8_t statusQryTimerId;
int8_t commandTtlTimerId;
/* EOF - State machine */

String strCommand;
boolean ISDEBUG             = false;

void setup() {
    Serial.begin(9600);
    Serial.println("Arduino Pentair Pool Pump Controller...");

    pinMode(SSerialTxControl, OUTPUT);
    pinMode(Pin13LED, OUTPUT);
    digitalWrite(Pin13LED, LOW);   // turn LED off

    // Init Transceiver, we start with listening
    digitalWrite(SSerialTxControl, RS485Receive);
    // Start the software serial port, to another device
    RS485Serial.begin(9600);   // set the data rate

    reset(); // Reset all variables to their default

    statusQryTimerId = timer1.every(STATUS_QRY_TIMEOUT, queryStatusCb);
    if (ISDEBUG) Serial.println("statusQryTimerId: " + String(statusQryTimerId));
}

void loop() {
    //region Serial command input
    // Parse a command
    if (Serial.available()) {
        int iRcv = -1;

        while ((iRcv = Serial.read()) > -1) {
            // Line ending indicates end of command
            if (iRcv == 0x0A || iRcv == 0x0D) {
                strCommand = String((char *)inputCmdBuffer);
                curCommand = strCommand.toInt();

                break;
            }

            char c = (char)iRcv;
            if (isDigit(c) == true) {
                *inputCmdBufPtr++ = c;
            }

            // Reset the buffer
            if ((inputCmdBufPtr - inputCmdBuffer) > MAX_INPUT_LEN) { // Pointer arithmetic usually gives you the index, but we increment the pointer after every assignment, so we get now the length
                memset(inputCmdBuffer, 0, sizeof(inputCmdBuffer));
                inputCmdBufPtr = inputCmdBuffer;

                break;
            }
        }
    }

    if (curCmdStage == IDLE && curCommand > CMD_NOOP) {
        switch(curCommand) {
            case 127:
                ISDEBUG = (ISDEBUG == true ? false : true);
                Serial.println("Command: Set ISDEBUG to " + String(ISDEBUG));
                return;
            case CMD_ALL_OFF:
                Serial.println("Command: Turning external programs off");
                pumpCommand = extProgOff;
                pumpCommandSz = sizeof(extProgOff);
                break;
            case CMD_RUN_PROG_1:
                Serial.println("Command: Running external program 1");
                pumpCommand = runExtProg1;
                pumpCommandSz = sizeof(runExtProg1);
                break;
            case CMD_RUN_PROG_2:
                Serial.println("Command: Running external program 2");
                pumpCommand = runExtProg2;
                pumpCommandSz = sizeof(runExtProg2);
                break;
            case CMD_RUN_PROG_3:
                Serial.println("Command: Running external program 3");
                pumpCommand = runExtProg3;
                pumpCommandSz = sizeof(runExtProg3);
                break;
            case CMD_RUN_PROG_4:
                Serial.println("Command: Running external program 4");
                pumpCommand = runExtProg4;
                pumpCommandSz = sizeof(runExtProg4);
                break;
            case CMD_STATUS:
                Serial.println("Command: Querying pump status");
                pumpCommand = getStatus;
                pumpCommandSz = sizeof(getStatus);
                break;
            case CMD_CTRL_REMOTE:
                Serial.println("Command: Setting control remote");
                pumpCommandSz = 0;
                curCmdStage = CTRL_REMOTE;
                break;
            case CMD_CTRL_LOCAL:
                Serial.println("Command: Setting control local");
                pumpCommandSz = 0;
                curCmdStage = CTRL_LOCAL;
                break;
            case CMD_PUMP_ON:
                Serial.println("Command: Turning pump on");
                pumpCommand = startPump;
                pumpCommandSz = sizeof(startPump);
                break;
            case CMD_PUMP_OFF:
                Serial.println("Command: Turning pump off");
                pumpCommand = stopPump;
                pumpCommandSz = sizeof(stopPump);
                break;
            default:
                Serial.print("What? Resetting....");
                reset();

                return;
        }

        curCmdStage = CTRL_REMOTE;
        commandTtlTimerId = timer1.after(COMMAND_TIMEOUT, commandTimeoutCb);
        memset(inputCmdBuffer, 0, sizeof(inputCmdBuffer));
        inputCmdBufPtr = inputCmdBuffer;
    }
    //endregion

    //region Send Ctrl Remote/Local or the actual Command
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
    //endregion

    //region Receive data from remote
    if ((curCmdStage == CONFIRM_REMOTE || curCmdStage == COMMAND_RESPONSE || curCmdStage == CONFIRM_LOCAL)) {
        if (RS485Serial.available() > 0) {
            int iRcv = -1;

            if (ISDEBUG) Serial.print("Incoming data: ");
            while ((iRcv = RS485Serial.read()) > -1) {
                *msgBufPtr++ = (char)iRcv;

                if (ISDEBUG) Serial.print(iRcv < 0x10 ? "0" + String(iRcv, HEX) : String(iRcv, HEX));

                // Reset the buffer, if we overflow. That should not happen
                if ((msgBufPtr - msgBuffer) > RCV_BUF_SZ) {
                    if (ISDEBUG) Serial.println("Buffer overflow. Resetting system...");
                    reset();

                    return;
                }
            }
            if (ISDEBUG) Serial.println();
        }

        // Now check if we got a useful message
        if (ISDEBUG) Serial.println("len: " + String((msgBufPtr - msgBuffer)) + ", min len: " + String((PREAMBLE_LEN - 1 + MIN_PKG_LEN)));
        int msgStartIdx = -1;
        int msgLength = -1;
        // FIXME: Handle multiple consecutive messages in buffer. Ideally make this a loop until no message
        boolean msgFound = findPAMessage(msgBuffer, (msgBufPtr - msgBuffer), &msgStartIdx, &msgLength);
        if (ISDEBUG) Serial.println("Message found: " + String(msgFound));

        if (msgFound == true) {
            uint8_t *relMsgPtr = &msgBuffer[msgStartIdx];
            // Reset the buffer and bail if the message was not for us
            if (relMsgPtr[MSG_DST_IDX] != MY_ADDRESS || relMsgPtr[MSG_SRC_IDX] != PUMP_ADDRESS) {
                if (ISDEBUG) Serial.println("Message not for us SRC: " + String(relMsgPtr[MSG_DST_IDX], HEX) + " DST: " + String(relMsgPtr[MSG_SRC_IDX], HEX));
                memset(msgBuffer, 0, sizeof(msgBuffer));
                msgBufPtr = msgBuffer;

                return;
            }

            // Response is not for our last command
            if (relMsgPtr[MSG_CFI_IDX] != lastCommand[MSG_CFI_IDX]) {
                if (ISDEBUG) Serial.println("Message is not a confirmation of our last command: SENT: " + String(lastCommand[MSG_CFI_IDX], HEX) + " RCVD: " + String(relMsgPtr[MSG_CFI_IDX], HEX));
                reset();

                return;
            }

            // Response should also contain the last command value, unless it was a status request
            if (lastCommand[MSG_CFI_IDX] == IFLO_CMD_STAT) {
                // TODO: Queue attribute updates
                figureOutChangedAttributes(relMsgPtr);
            } else {
                for (int i = MSG_LEN_IDX + 1; i < relMsgPtr[MSG_LEN_IDX]; ++i) {
                    uint8_t lCmdVal = 0x00;

                    // Register writes echo only the value!
                    if (lastCommand[MSG_CFI_IDX] == IFLO_CMD_REG) {
                        lCmdVal = lastCommand[i + sizeof(IFLO_REG_EPRG)];
                    } else {
                        lCmdVal = lastCommand[i];
                    }

                    if (relMsgPtr[i] != lCmdVal) {
                        if (ISDEBUG)Serial.println("Message does not contain expected value of sent command value at data index " + String(i) + " SENT: " + String(lCmdVal) + " RCVD: " + String(relMsgPtr[i]));
                        reset();

                        return;
                    }
                }
            }

            Serial.println("Last command confirmed");

            // Reset the receiving buffer
            memset(msgBuffer, 0, sizeof(msgBuffer));
            msgBufPtr = msgBuffer;
            timer1.stop(commandTtlTimerId);
            commandTtlTimerId = -1;

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
        }
    }
    //endregion

    timer1.update();
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

    if (ISDEBUG) {
        Serial.println("msgStartIdx: " + String(*msgStartIdx));
        Serial.println("actualMsgLength: " + String(*actualMsgLength));
        Serial.println("Calc checksum: " + String(chkSum));
        Serial.println("High byte: " + String(data[*msgStartIdx + *actualMsgLength - 2]));
        Serial.println("Low byte: " + String(data[*msgStartIdx + *actualMsgLength - 1]));
        Serial.println("Exp checksum: " + String(expChkSum));
    }

    // Verify the checksum
    if (chkSum != expChkSum) {
        if (ISDEBUG) Serial.println("Checksum is: BAD");

        return false;
    }

    if (ISDEBUG) Serial.println("Checksum is: GOOD");
    return true;
}

void figureOutChangedAttributes(uint8_t *statusMsg) {
    String statOutput = "";
    if (ISDEBUG) statOutput += "Pump status: \n";

    if (statusMsg[STAT_RUN_IDX] != pumpStatus.running) {
        pumpStatus.running = statusMsg[STAT_RUN_IDX];
        if (ISDEBUG) statOutput += "\tRUN: " + String(statusMsg[STAT_RUN_IDX]) + "\n";
    }

    if (statusMsg[STAT_MODE_IDX] != pumpStatus.mode) {
        pumpStatus.mode = statusMsg[STAT_MODE_IDX];
        if (ISDEBUG) statOutput += "\tMOD: " + String(statusMsg[STAT_MODE_IDX]) + "\n";
    }

    if (statusMsg[STAT_STATE_IDX] != pumpStatus.drive_state) {
        pumpStatus.drive_state = statusMsg[STAT_STATE_IDX];
        if (ISDEBUG) statOutput += "\tSTE: " + String(statusMsg[STAT_STATE_IDX]) + "\n";
    }

    uint16_t pwr_usage = (statusMsg[STAT_PWR_HB_IDX] * 256) + statusMsg[STAT_PWR_LB_IDX];
    if (pwr_usage != pumpStatus.pwr_usage) {
        pumpStatus.pwr_usage = pwr_usage;
        if (ISDEBUG) statOutput += "\tPWR: " + String(pwr_usage) + " WATT" + "\n";
    }

    uint16_t speed = (statusMsg[STAT_RPM_HB_IDX] * 256) + statusMsg[STAT_RPM_LB_IDX];
    if (pwr_usage != pumpStatus.speed) {
        pumpStatus.speed = speed;
        if (ISDEBUG) statOutput += "\tRPM: " + String(speed) + " RPM" + "\n";
    }

    if (statusMsg[STAT_GPM_IDX] != pumpStatus.flow_rate) {
        pumpStatus.flow_rate = statusMsg[STAT_GPM_IDX];
        if (ISDEBUG) statOutput += "\tGPM: " + String(statusMsg[STAT_GPM_IDX]) + " GPM" + "\n";
    }

    if (statusMsg[STAT_PPC_IDX] != pumpStatus.ppc_levels) {
        pumpStatus.ppc_levels = statusMsg[STAT_PPC_IDX];
        if (ISDEBUG) statOutput += "\tPPC: " + String(statusMsg[STAT_PPC_IDX]) + " %" + "\n";
    }

    if (statusMsg[STAT_B09_IDX] != pumpStatus.b09) {
        pumpStatus.b09 = statusMsg[STAT_B09_IDX];
        if (ISDEBUG) statOutput += "\tB09: " + String(statusMsg[STAT_B09_IDX]) + "\n";
    }

    if (statusMsg[STAT_ERR_IDX] != pumpStatus.error_code) {
        pumpStatus.error_code = statusMsg[STAT_ERR_IDX];
        if (ISDEBUG) statOutput += "\tERR: " + String(statusMsg[STAT_ERR_IDX]) + "\n";
    }

    if (statusMsg[STAT_B11_IDX] != pumpStatus.b11) {
        pumpStatus.b11 = statusMsg[STAT_B11_IDX];
        if (ISDEBUG) statOutput += "\tB11: " + String(statusMsg[STAT_B11_IDX]) + "\n";
    }

    if (statusMsg[STAT_TIMER_IDX] != pumpStatus.timer) {
        pumpStatus.timer = statusMsg[STAT_TIMER_IDX];
        if (ISDEBUG) statOutput += "\tTMR: " + String(statusMsg[STAT_TIMER_IDX]) + " MIN" + "\n";
    }

    char clk[6] = "--:--";
    clk[0] = '0' + statusMsg[STAT_CLK_HOUR_IDX] / 10;
    clk[1] = '0' + statusMsg[STAT_CLK_HOUR_IDX] % 10;
    clk[3] = '0' + statusMsg[STAT_CLK_MIN_IDX] / 10;
    clk[4] = '0' + statusMsg[STAT_CLK_MIN_IDX] % 10;

    if (strcmp(clk, pumpStatus.clock) != 0) {
        strcpy(pumpStatus.clock, clk);
        if (ISDEBUG) statOutput += "\tCLK: " + String(clk) + "\n";
    }

    if (ISDEBUG) {
        Serial.print(statOutput);
    }
}

void reset() {
    // Stop the timer
    timer1.stop(commandTtlTimerId);

    memset(inputCmdBuffer, 0, sizeof(inputCmdBuffer));
    inputCmdBufPtr = inputCmdBuffer;
    memset(msgBuffer, 0, sizeof(msgBuffer));
    msgBufPtr = msgBuffer;
    pumpCommandSz = 0;
    lastCommandSz = 0;
    curCmdStage = IDLE;
    curCommand = CMD_NOOP;
    commandTtlTimerId = -1;
}

/*
 * Callback to query pump status every 15 seconds, if we are idle
 */
void queryStatusCb() {
    Serial.println("Status query callback executing... curCmdStage: " + String(curCmdStage));

    if (curCmdStage == IDLE) {
        curCommand = CMD_STATUS;
    }
}

/*
 * Callback that will reset the message buffers, if we haven't received a complete response from the pump within a given timewindow
 */
void commandTimeoutCb() {
    Serial.println("Command timeout callback executing... curCmdStage: " + String(curCmdStage) + ", message buffer size: " + String((msgBufPtr - msgBuffer)));

    if (curCmdStage != IDLE) {
        reset();
    }
}