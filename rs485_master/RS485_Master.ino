/* This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://www.wtfpl.net/ for more details.
 */

#include <SoftwareSerial.h>
#include <Timer.h>
#include <SPI.h>
#include <iafLib.h>
#include <ArduinoSPI.h>
#include "profile/device-description.h"
#include "CommandQueue.h"

//region Pump commands and instructions
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
// TODO: We'll need a queue to handle the following commands as we can't release control of the pump until all settings have bee set and the pump turned on
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
#define	IFLO_MODE_EXT_P1	0x09 // Run Ext. Prog. 1 //Erie: Is this maybe speed 1 thru 4 ??? These command don't seem to have a "All programs off command"
#define	IFLO_MODE_EXT_P2	0x0A // Run Ext. Prog. 2
#define	IFLO_MODE_EXT_P3	0x0B // Run Ext. Prog. 3
#define	IFLO_MODE_EXT_P4	0x0C // Run Ext. Prog. 4

#define IFLO_CMD_RUN        0x06
#define IFLO_RUN_STRT   	0x0A
#define IFLO_RUN_STOP   	0x04

// Is supposedly reported on byte 3, but it's alwasy 0x00 in my case. It may depend on the system setup or pump model
#define IFLO_STATE_PRIMING  0x01
#define IFLO_STATE_RUNNING  0x02
#define IFLO_STATE_SYS_PRIMING  0x04

#define IFLO_CMD_STAT       0x07
//endregion Pump commands and instructions

//region Pump message structure related stuff like indexes, constants, etc.
/*
   Every device on the bus has an address:
       0x0f - is the broadcast address, it is used by the more sophisticated controllers as <dst>
              in their system status broadcasts most likely to keep queries for system status low.
       0x1x - main controllers (IntelliComII, IntelliTouch, EasyTouch ...)
       0x2x - remote controllers
       0x6x - pumps, 0x60 is pump 1

   Let's use 0x20, the first address in the remote controller space
 */
uint8_t pumpAddress         = 0x60;
uint8_t controllerAddress   = 0x20;

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

//                                             P R E A M B L E                     VER   DST          SRC                CFI               LEN   DAT               CHB   CLB (Check Sum is set when sending command, depends on addresses used)
uint8_t cmdArrCtrlRemote[]  = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, pumpAddress, controllerAddress, IFLO_CMD_CTRL,    0x01, IFLO_CTRL_REMOTE, 0x00, 0x00};
uint8_t cmdArrCtrlLocal[]   = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, pumpAddress, controllerAddress, IFLO_CMD_CTRL,    0x01, IFLO_CTRL_LOCAL,  0x00, 0x00};
uint8_t cmdArrGetStatus[]   = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, pumpAddress, controllerAddress, IFLO_CMD_STAT,    0x00,                        0x00, 0x00};
uint8_t cmdArrExtProgOff[]  = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, pumpAddress, controllerAddress, IFLO_CMD_REG,     0x04, (IFLO_REG_EPRG >> 8), (IFLO_REG_EPRG & 0xFF), (IFLO_EPRG_OFF >> 8), (IFLO_EPRG_OFF & 0xFF), 0x00, 0x00};
uint8_t cmdArrRunExtProg1[] = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, pumpAddress, controllerAddress, IFLO_CMD_REG,     0x04, (IFLO_REG_EPRG >> 8), (IFLO_REG_EPRG & 0xFF), (IFLO_EPRG_P1 >> 8), (IFLO_EPRG_P1 & 0xFF), 0x00, 0x00};
uint8_t cmdArrRunExtProg2[] = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, pumpAddress, controllerAddress, IFLO_CMD_REG,     0x04, (IFLO_REG_EPRG >> 8), (IFLO_REG_EPRG & 0xFF), (IFLO_EPRG_P2 >> 8), (IFLO_EPRG_P2 & 0xFF), 0x00, 0x00};
uint8_t cmdArrRunExtProg3[] = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, pumpAddress, controllerAddress, IFLO_CMD_REG,     0x04, (IFLO_REG_EPRG >> 8), (IFLO_REG_EPRG & 0xFF), (IFLO_EPRG_P3 >> 8), (IFLO_EPRG_P3 & 0xFF), 0x00, 0x00};
uint8_t cmdArrRunExtProg4[] = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, pumpAddress, controllerAddress, IFLO_CMD_REG,     0x04, (IFLO_REG_EPRG >> 8), (IFLO_REG_EPRG & 0xFF), (IFLO_EPRG_P4 >> 8), (IFLO_EPRG_P4 & 0xFF), 0x00, 0x00};
uint8_t cmdArrSetSpeed1[]   = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, pumpAddress, controllerAddress, IFLO_CMD_MODE,    0x01, IFLO_MODE_EXT_P1, 0x00, 0x00};
uint8_t cmdArrSetSpeed2[]   = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, pumpAddress, controllerAddress, IFLO_CMD_MODE,    0x01, IFLO_MODE_EXT_P2, 0x00, 0x00};
uint8_t cmdArrSetSpeed3[]   = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, pumpAddress, controllerAddress, IFLO_CMD_MODE,    0x01, IFLO_MODE_EXT_P3, 0x00, 0x00};
uint8_t cmdArrSetSpeed4[]   = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, pumpAddress, controllerAddress, IFLO_CMD_MODE,    0x01, IFLO_MODE_EXT_P4, 0x00, 0x00};
uint8_t cmdArrStartPump[]   = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, pumpAddress, controllerAddress, IFLO_CMD_RUN,     0x01, IFLO_RUN_STRT,    0x00, 0x00};
uint8_t cmdArrStopPump[]    = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, pumpAddress, controllerAddress, IFLO_CMD_RUN,     0x01, IFLO_RUN_STOP,    0x00, 0x00};
//endregion Pump message structure related stuff like indexes, constants, etc.

//region afLib stuff
// Automatically detect if we are on Teensy or UNO.
#if defined(ARDUINO_AVR_UNO)
#define INT_PIN                   2
#define CS_PIN                    10

#elif defined(ARDUINO_AVR_MEGA2560)
#define INT_PIN                   2
#define CS_PIN                    10
#define RESET                     21    // This is used to reboot the Modulo when the Teensy boots

// Need to define these as inputs so they will float and we can connect the real pins for SPI on the
// 2560 which are 50 - 52. You need to use jumper wires to connect the pins from the 2560 to the Plinto.
// Since the 2560 is 5V, you must use a Plinto adapter.
#define MOSI                      11    // 51 on Mega 2560
#define MISO                      12    // 50 on Mega 2560
#define SCK                       13    // 52 on Mega 2560

#elif defined(TEENSYDUINO)
#define INT_PIN                   14    // Modulo uses this to initiate communication
#define CS_PIN                    10    // Standard SPI chip select (aka SS)
#define RESET                     21    // This is used to reboot the Modulo when the Teensy boots
#else
#error "Sorry, afLib does not support this board"
#endif

iafLib *aflib;
//endregion

//region Function declarations
void queuePumpInstruction(const uint8_t *instruction);
CommandStruct *buildCommandStruct(const uint8_t command, size_t size);
boolean findPAMessage(const uint8_t *data, const int len, int *msgStartIdx, int *actualMsgLength);
void figureOutChangedAttributes(const uint8_t *statusMsg);
void reset();

void queryStatusCb();
void commandTimeoutCb();

void onAttrSet(const uint8_t requestId, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value);
void onAttrSetComplete(const uint8_t requestId, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value);
//endregion

//region Serial command input
#define MAX_INPUT_LEN       16
uint8_t inputCmdBuffer[MAX_INPUT_LEN];
uint8_t *inputCmdBufPtr;
//endregion Serial command input

//region RS485 message processing
#define SSerialTx           8 // DI: data in
#define SSerialTxControl    9 // DE: data enable
#define RE_PIN              10 // RE: receive enable; jumpered together with 6, will be put into floating mode
#define SSerialRx           11 // RO: receive out
#define RS485Transmit       HIGH
#define RS485Receive        LOW

#define Pin13LED            13

SoftwareSerial RS485Serial(SSerialRx, SSerialTx); // RX, TX

// The theoretical maximum is 264 bytes. however, it seems that actual message are much shorter, i.e. 37 bytes was
// the longest seen so far. So, allocate 2x size of an expected message size: 2 x 32 bytes.
//
//#define MAX_PKG_LEN         6+256+2 //Msg Prefix + Data + Checksum
#define MAX_PKG_LEN         6 + 32 + 2 // 40 bytes
#define MIN_PKG_LEN         6 + 0 + 2 // 8 bytes
#define RCV_BUF_SZ          (3 + MAX_PKG_LEN) * 2 // 86 bytes, includes preamble
uint8_t msgBuffer[RCV_BUF_SZ];
uint8_t *msgBufPtr;
//endregion RS485 message processing

//region State machine
/* 1. Send Remote Ctrl
// 1.1 Wait for confirmation
// 2. Send Command
// 2.1 Process confirmation/response, i.e. status
// 3. Send Local Ctrl
// 3.1 Wait for confirmation */
enum COMMAND_STAGE {
    CMD_STAGE_IDLE,
    CMD_STAGE_SEND,
    CMD_STAGE_CONFIRM
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

enum CONTROL_MODE {
    CTRL_MODE_REMOTE = IFLO_CTRL_REMOTE,
    CTRL_MODE_LOCAL = IFLO_CTRL_LOCAL
};

struct PumpStatus {
    uint8_t ctrl_mode;
    uint8_t pumpAddr;
    uint8_t ctlrAddr;
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

COMMAND_STAGE commandStage;
CommandQueue *commandQueue;
PumpStatus pumpStatusStruct;

#define STATUS_QRY_TIMEOUT  1000 * 15   // 15 seconds
#define COMMAND_TIMEOUT     1000 * 2    // 2 second
Timer timer1;
int8_t statusQryTimerId;
int8_t commandTtlTimerId;
//endregion State machine

String strCommand;
boolean ISDEBUG             = false;

//region Queue test
//void setup() {
//    Serial.begin(9600);
//    while (!Serial) {
//        ;
//    }
//
//    Serial.println("Command Queue Test....");
//
//    uint16_t chkSum = 0;
//    size_t cmdArrLen = 0;
//    CommandQueue pumpCommandQueue;
//    CommandStruct *commandStruct = (CommandStruct *)malloc(sizeof(CommandStruct));
//
//    cmdArrLen = sizeof(cmdArrCtrlRemote);
//    commandStruct->command = (uint8_t *)malloc(cmdArrLen);
//    commandStruct->size = cmdArrLen;
//    memset(commandStruct->command, 0, cmdArrLen);
//
//    // Complete command
//    Serial.println("Queueing command #1");
//    Serial.print("Data: ");
//    for(int i = 0; i < cmdArrLen; ++i) {
//        commandStruct->command[i] = cmdArrCtrlRemote[i];
//
//        if (i == MSG_SRC_IDX) {
//            commandStruct->command[i] = controllerAddress;
//        } else if (i == MSG_DST_IDX) {
//            commandStruct->command[i] = pumpAddress;
//        } else if (i < cmdArrLen - 2) {
//            chkSum += commandStruct->command[i];
//        } else if (i == cmdArrLen - 2) {
//            commandStruct->command[i] = chkSum >> 8;
//        } else if (i == cmdArrLen - 1) {
//            commandStruct->command[i] = chkSum & 0xFF;
//        }
//
//        Serial.print(commandStruct->command[i] < 0x10 ? "0" + String(commandStruct->command[i], HEX) : String(commandStruct->command[i], HEX));
//    }
//    Serial.println();
//    pumpCommandQueue.Enqueue(commandStruct);
//
//    // Complete second command
//    chkSum = 0;
//    cmdArrLen = sizeof(cmdArrGetStatus);
//    commandStruct = (CommandStruct *)malloc(cmdArrLen);
//    commandStruct->command = (uint8_t *)malloc(cmdArrLen);
//    commandStruct->size = cmdArrLen;
//    memset(commandStruct->command, 0, cmdArrLen);
//
//    Serial.println("Queueing command #2");
//    Serial.print("Data: ");
//    for(int i = 0; i < cmdArrLen; ++i) {
//        commandStruct->command[i] = cmdArrGetStatus[i];
//
//        if (i == MSG_SRC_IDX) {
//            commandStruct->command[i] = controllerAddress;
//        } else if (i == MSG_DST_IDX) {
//            commandStruct->command[i] = pumpAddress;
//        } else if (i < cmdArrLen - 2) {
//            chkSum += commandStruct->command[i];
//        } else if (i == cmdArrLen - 2) {
//            commandStruct->command[i] = chkSum >> 8;
//        } else if (i == cmdArrLen - 1) {
//            commandStruct->command[i] = chkSum & 0xFF;
//        }
//
//        Serial.print(commandStruct->command[i] < 0x10 ? "0" + String(commandStruct->command[i], HEX) : String(commandStruct->command[i], HEX));
//    }
//    Serial.println();
//    pumpCommandQueue.Enqueue(commandStruct);
//
//    Serial.println("Size: " + String(pumpCommandQueue.GetSize()));
//
//    uint8_t cmdCnt = 0;
//    Serial.println("pumpCommandQueue.hasNext(): " + String(pumpCommandQueue.hasNext()));
//    while (pumpCommandQueue.hasNext() == true) {
//        commandStruct = pumpCommandQueue.Dequeue();
//        cmdArrLen = commandStruct->size;
//        Serial.println("Printing command #" + String(++cmdCnt));
//        Serial.print("Data: ");
//        for (int i = 0; i < cmdArrLen; ++i) {
//            Serial.print(commandStruct->command[i] < 0x10 ? "0" + String(commandStruct->command[i], HEX) : String(commandStruct->command[i], HEX));
//        }
//        Serial.println();
//    }
//
//    // Complete third command
//    chkSum = 0;
//    cmdArrLen = sizeof(cmdArrCtrlLocal);
//    commandStruct = (CommandStruct *)malloc(cmdArrLen);
//    commandStruct->command = (uint8_t *)malloc(cmdArrLen);
//    commandStruct->size = cmdArrLen;
//    memset(commandStruct->command, 0, cmdArrLen);
//
//    Serial.println("Queueing command #2");
//    Serial.print("Data: ");
//    for(int i = 0; i < cmdArrLen; ++i) {
//        commandStruct->command[i] = cmdArrCtrlLocal[i];
//
//        if (i == MSG_SRC_IDX) {
//            commandStruct->command[i] = controllerAddress;
//        } else if (i == MSG_DST_IDX) {
//            commandStruct->command[i] = pumpAddress;
//        } else if (i < cmdArrLen - 2) {
//            chkSum += commandStruct->command[i];
//        } else if (i == cmdArrLen - 2) {
//            commandStruct->command[i] = chkSum >> 8;
//        } else if (i == cmdArrLen - 1) {
//            commandStruct->command[i] = chkSum & 0xFF;
//        }
//
//        Serial.print(commandStruct->command[i] < 0x10 ? "0" + String(commandStruct->command[i], HEX) : String(commandStruct->command[i], HEX));
//    }
//    Serial.println();
//    pumpCommandQueue.Enqueue(commandStruct);
//
//    Serial.println("Size: " + String(pumpCommandQueue.GetSize()));
//
//    cmdCnt = 0;
//    Serial.println("pumpCommandQueue.hasNext(): " + String(pumpCommandQueue.hasNext()));
//    while (pumpCommandQueue.hasNext() == true) {
//        commandStruct = pumpCommandQueue.Dequeue();
//        cmdArrLen = commandStruct->size;
//        Serial.println("Printing command #" + String(++cmdCnt));
//        Serial.print("Data: ");
//        for (int i = 0; i < cmdArrLen; ++i) {
//            Serial.print(commandStruct->command[i] < 0x10 ? "0" + String(commandStruct->command[i], HEX) : String(commandStruct->command[i], HEX));
//        }
//        Serial.println();
//    }
//}
//
//void loop() {
//
//}
//endregion

void setup() {
    Serial.begin(9600);
    while (!Serial) {
        ;
    }

    Serial.println("Arduino Pentair Pool Pump Controller...");

    pinMode(SSerialTxControl, OUTPUT);
    pinMode(Pin13LED, OUTPUT);
    digitalWrite(Pin13LED, LOW);   // turn LED off

    // Init Transceiver, we start with listening
    digitalWrite(SSerialTxControl, RS485Receive);
    // Start the software serial port, to another device
    RS485Serial.begin(9600);   // set the data rate

    //region Arduino Board setup
    // The Plinto board automatically connects reset on UNO to reset on Modulo
    // For Teensy, we need to reset manually...
#if defined(TEENSYDUINO)
    Serial.println("Using Teensy - Resetting Modulo");
    pinMode(RESET, OUTPUT);
    digitalWrite(RESET, 0);
    delay(250);
    digitalWrite(RESET, 1);
#endif

#if defined(ARDUINO_AVR_MEGA2560)
    Serial.println("Using MEGA2560 - Resetting Modulo");

    // Allow the Plinto SPI pins to float, we'll drive them from elsewhere
    pinMode(MOSI, INPUT);
    pinMode(MISO, INPUT);
    pinMode(SCK, INPUT);

    pinMode(RESET, OUTPUT);
    digitalWrite(RESET, 0);
    delay(250);
    digitalWrite(RESET, 1);
    delay(1000);
#endif
    //endregion Arduino Board setup

    // TODO: Do we need this?
    ArduinoSPI *arduinoSPI = new ArduinoSPI(CS_PIN);

    /**
    * Initialize the afLib
    *
    * Just need to configure a few things:
    *  INT_PIN - the pin used slave interrupt
    *  ISRWrapper - function to pass interrupt on to afLib
    *  onAttrSet - the function to be called when one of your attributes has been set.
    *  onAttrSetComplete - the function to be called in response to a getAttribute call or when a afero attribute has been updated.
    *  Serial - class to handle serial communications for debug output.
    *  theSPI - class to handle SPI communications.
    */
    aflib = iafLib::create(digitalPinToInterrupt(INT_PIN), ISRWrapper, onAttrSet, onAttrSetComplete, &Serial, arduinoSPI);

    //Initialize the pump status
    pumpStatusStruct.ctrl_mode = CTRL_MODE_LOCAL;
    pumpStatusStruct.pumpAddr = pumpAddress;
    pumpStatusStruct.ctlrAddr = controllerAddress;

    memset(inputCmdBuffer, 0, sizeof(inputCmdBuffer));
    inputCmdBufPtr = inputCmdBuffer;

    reset(); // Reset all variables to their default

    // Start a status query timer
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
                uint8_t c = strCommand.toInt();
                queuePumpInstruction(&c);

                memset(inputCmdBuffer, 0, sizeof(inputCmdBuffer));
                inputCmdBufPtr = inputCmdBuffer;

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
    //endregion

    //region Send Ctrl Remote/Local or the actual Command
    if (commandStage == CMD_STAGE_SEND) {
        digitalWrite(SSerialTxControl, RS485Transmit);

        const CommandStruct *cmd = commandQueue->Peek();

        if (ISDEBUG) {
            Serial.print("Sending data: ");
            for (int i = 0; i < cmd->size; ++i) {
                if (ISDEBUG)
                    Serial.print(
                            cmd->command[i] < 0x10 ? "0" + String(cmd->command[i], HEX) : String(cmd->command[i], HEX));
            }
            Serial.println(", Checksum: " + String(cmd->command[cmd->size - 2] < 8 + cmd->command[cmd->size - 1]));
        }

        RS485Serial.write(cmd->command, cmd->size);

        digitalWrite(SSerialTxControl, RS485Receive);

        commandTtlTimerId = timer1.after(COMMAND_TIMEOUT, commandTimeoutCb);

        //Advance the command stage
        commandStage = CMD_STAGE_CONFIRM;
    }
    //endregion

    //region Receive data from remote
    if (commandStage == CMD_STAGE_CONFIRM) {
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
            const CommandStruct *lastCommand = commandQueue->Peek();
            uint8_t *relMsgPtr = &msgBuffer[msgStartIdx];
            // Reset the buffer and bail if the message was not for us
            if (relMsgPtr[MSG_DST_IDX] != controllerAddress || relMsgPtr[MSG_SRC_IDX] != pumpAddress) {
                if (ISDEBUG) Serial.println("Message not for us SRC: " + String(relMsgPtr[MSG_DST_IDX], HEX) + " DST: " + String(relMsgPtr[MSG_SRC_IDX], HEX));
                memset(msgBuffer, 0, sizeof(msgBuffer));
                msgBufPtr = msgBuffer;

                return;
            }

            // Response is not for our last command
            if (relMsgPtr[MSG_CFI_IDX] != lastCommand->command[MSG_CFI_IDX]) {
                if (ISDEBUG) Serial.println("Message is not a confirmation of our last command: SENT: "
                                            + String(lastCommand->command[MSG_CFI_IDX], HEX) + " RCVD: " + String(relMsgPtr[MSG_CFI_IDX], HEX));
                reset();

                return;
            }

            // Response should also contain the last command value, unless it was a status request
            if (lastCommand->command[MSG_CFI_IDX] == IFLO_CMD_STAT) {
                // TODO: Queue attribute updates
                figureOutChangedAttributes(relMsgPtr);
            } else {
                for (int i = MSG_LEN_IDX + 1; i < relMsgPtr[MSG_LEN_IDX]; ++i) {
                    uint8_t lCmdVal = 0x00;

                    // Register writes echo only the value!
                    if (lastCommand->command[MSG_CFI_IDX] == IFLO_CMD_REG) {
                        lCmdVal = lastCommand->command[i + sizeof(IFLO_REG_EPRG)];
                    } else {
                        lCmdVal = lastCommand->command[i];
                    }

                    if (relMsgPtr[i] != lCmdVal) {
                        if (ISDEBUG)Serial.println("Message does not contain expected value of sent command value at data index "
                                                   + String(i) + " SENT: " + String(lCmdVal) + " RCVD: " + String(relMsgPtr[i]));
                        reset();

                        return;
                    }
                }
            }

            Serial.println("Last command confirmed");

            // Reset the receiving buffer and dispose of the last command
            memset(msgBuffer, 0, sizeof(msgBuffer));
            msgBufPtr = msgBuffer;
            timer1.stop(commandTtlTimerId);
            commandTtlTimerId = -1;
            CommandStruct *tmpCmdStruct = commandQueue->Dequeue();
            free(tmpCmdStruct->command);
            free(tmpCmdStruct);

            // We should be good now
            if (commandQueue->HasNext() == true) {
                commandStage = CMD_STAGE_SEND;
            } else {
                commandStage = CMD_STAGE_IDLE;
            }
        }
    }
    //endregion

    timer1.update();
    aflib->loop();
}

/*
 * Build the queue of instructions we need to send to the pump to make the current command work.
 * For example, if we want to run Program 1, we need to
 * 1. Put the pump into remote control
 * 1.1 Confirm remote control
 * 2. Set program(s), write memory or set mode
 * 2.1 Confirm commands that are in the queue
 * 3. repeat 2. until everything is set
 * 4. Put the pump into local control mode again
 *
 * TODO: Keep track of remote/local control setting.
 */
void queuePumpInstruction(const uint8_t *instruction) {
    if (commandStage == CMD_STAGE_IDLE && *instruction > CMD_NOOP) {
        CommandStruct *commandStruct;

        // Fill the command queue
        //1. Set the pump into remote control mode, if it's not in remote control mode yet
        if (pumpStatusStruct.ctrl_mode == CTRL_MODE_LOCAL && *instruction != CMD_CTRL_REMOTE && *instruction != CMD_CTRL_LOCAL) {
            commandStruct = buildCommandStruct(cmdArrCtrlRemote, sizeof(cmdArrCtrlRemote));
            commandQueue->Enqueue(commandStruct);
        }

        switch(*instruction) {
            case 127:
                ISDEBUG = (ISDEBUG == true ? false : true);
                Serial.println("Command: Set ISDEBUG to " + String(ISDEBUG));
                return;
//            case CMD_ALL_OFF:
//                Serial.println("Command: Turning external programs off");
//                commandStruct = buildCommandStruct(cmdArrExtProgOff, sizeof(cmdArrExtProgOff));
//                commandQueue->Enqueue(commandStruct);
//                break;
            case CMD_RUN_PROG_1:
                Serial.println("Command: Running external program 1");
                commandStruct = buildCommandStruct(cmdArrSetSpeed1, sizeof(cmdArrSetSpeed1));
                commandQueue->Enqueue(commandStruct);

                // Queue the start command, if the pump is not running yet
                if (pumpStatusStruct.mode == IFLO_RUN_STOP) {
                    commandStruct = buildCommandStruct(cmdArrStartPump, sizeof(cmdArrStartPump));
                    commandQueue->Enqueue(commandStruct);
                }
                break;
            case CMD_RUN_PROG_2:
                Serial.println("Command: Running external program 2");
                commandStruct = buildCommandStruct(cmdArrSetSpeed2, sizeof(cmdArrSetSpeed2));
                commandQueue->Enqueue(commandStruct);

                // Queue the start command, if the pump is not running yet
                if (pumpStatusStruct.mode == IFLO_RUN_STOP) {
                    commandStruct = buildCommandStruct(cmdArrStartPump, sizeof(cmdArrStartPump));
                    commandQueue->Enqueue(commandStruct);
                }
                break;
            case CMD_RUN_PROG_3:
                Serial.println("Command: Running external program 3");
                commandStruct = buildCommandStruct(cmdArrSetSpeed3, sizeof(cmdArrSetSpeed3));
                commandQueue->Enqueue(commandStruct);

                // Queue the start command, if the pump is not running yet
                if (pumpStatusStruct.mode == IFLO_RUN_STOP) {
                    commandStruct = buildCommandStruct(cmdArrStartPump, sizeof(cmdArrStartPump));
                    commandQueue->Enqueue(commandStruct);
                }
                break;
            case CMD_RUN_PROG_4:
                Serial.println("Command: Running external program 4");
                commandStruct = buildCommandStruct(cmdArrSetSpeed4, sizeof(cmdArrSetSpeed4));
                commandQueue->Enqueue(commandStruct);

                // Queue the start command, if the pump is not running yet
                if (pumpStatusStruct.mode == IFLO_RUN_STOP) {
                    commandStruct = buildCommandStruct(cmdArrStartPump, sizeof(cmdArrStartPump));
                    commandQueue->Enqueue(commandStruct);
                }
                break;
            case CMD_STATUS:
                Serial.println("Command: Querying pump status");
                commandStruct = buildCommandStruct(cmdArrGetStatus, sizeof(cmdArrGetStatus));
                commandQueue->Enqueue(commandStruct);
                break;
            case CMD_CTRL_REMOTE:
                Serial.println("Command: Setting control remote");
                commandStruct = buildCommandStruct(cmdArrCtrlRemote, sizeof(cmdArrCtrlRemote));
                commandQueue->Enqueue(commandStruct);
                pumpStatusStruct.ctrl_mode = CTRL_MODE_REMOTE;
                break;
            case CMD_CTRL_LOCAL:
                Serial.println("Command: Setting control local");
                commandStruct = buildCommandStruct(cmdArrCtrlLocal, sizeof(cmdArrCtrlLocal));
                commandQueue->Enqueue(commandStruct);
                pumpStatusStruct.ctrl_mode = CTRL_MODE_LOCAL;
                break;
//            case CMD_PUMP_ON:
//                Serial.println("Command: Turning pump on");
//                commandStruct = buildCommandStruct(cmdArrStartPump, sizeof(cmdArrStartPump));
//                commandQueue->Enqueue(commandStruct);
//                break;
//            case CMD_PUMP_OFF:
//                Serial.println("Command: Turning pump off");
//                commandStruct = buildCommandStruct(cmdArrStopPump, sizeof(cmdArrStopPump));
//                commandQueue->Enqueue(commandStruct);
//                break;
            default:
                Serial.print("What? Resetting....");
                reset();

                return;
        }

        // Set the pump into local control mode, if it has not been set into explicit remote control mode
        if (pumpStatusStruct.ctrl_mode == CTRL_MODE_LOCAL && *instruction != CMD_CTRL_REMOTE && *instruction != CMD_CTRL_LOCAL) {
            CommandStruct *commandStruct = buildCommandStruct(cmdArrCtrlLocal, sizeof(cmdArrCtrlLocal));
            commandQueue->Enqueue(commandStruct);
        }

        commandStage = CMD_STAGE_SEND;
    }
}

CommandStruct *buildCommandStruct(uint8_t *command, size_t size) {
    uint16_t chkSum = 0;
    CommandStruct *commandStruct = (CommandStruct *) malloc(sizeof(CommandStruct));

    commandStruct->command = (uint8_t *) malloc(size);
    commandStruct->size = size;

    Serial.println("Queueing command #1");
    Serial.print("Data: ");
    for (int i = 0; i < size; ++i) {
        commandStruct->command[i] = command[i];

        if (i >= MSG_BGN_IDX && i < size - 2) {
            chkSum += commandStruct->command[i]; // Calculate the checksum
        } else if (i == MSG_SRC_IDX) {
            commandStruct->command[i] = controllerAddress;
        } else if (i == MSG_DST_IDX) {
            commandStruct->command[i] = pumpAddress;
        } else if (i == size - 2) {
            commandStruct->command[i] = chkSum >> 8; // Set checksum high byte
        } else if (i == size - 1) {
            commandStruct->command[i] = chkSum & 0xFF; // Set checksum low byte
        }

        Serial.print(commandStruct->command[i] < 0x10 ? "0" + String(commandStruct->command[i], HEX) : String(commandStruct->command[i], HEX));
    }
    Serial.println();

    return commandStruct;
}

boolean findPAMessage(const uint8_t *data, const int len, int *msgStartIdx, int *actualMsgLength) {
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

void figureOutChangedAttributes(const uint8_t *statusMsg) {
    String statOutput = "";
    if (ISDEBUG) statOutput += "Pump status: \n";

    if (statusMsg[STAT_RUN_IDX] != pumpStatusStruct.running) {
        pumpStatusStruct.running = statusMsg[STAT_RUN_IDX];
        if (ISDEBUG) statOutput += "\tRUN: " + String(statusMsg[STAT_RUN_IDX]) + "\n";
        if (aflib->setAttribute8(AF_STATUS__PUMP_RUNNING_STATE, pumpStatusStruct.running) != afSUCCESS) {
            if (ISDEBUG) Serial.println("Couldn't set running status attribute");
        }
    }

    if (statusMsg[STAT_MODE_IDX] != pumpStatusStruct.mode) {
        pumpStatusStruct.mode = statusMsg[STAT_MODE_IDX];
        if (ISDEBUG) statOutput += "\tMOD: " + String(statusMsg[STAT_MODE_IDX]) + "\n";
        if (aflib->setAttribute8(AF_STATUS__PUMP_MODE, pumpStatusStruct.mode) != afSUCCESS) {
            if (ISDEBUG) Serial.println("Couldn't set mode attribute");
        }
    }

    if (statusMsg[STAT_STATE_IDX] != pumpStatusStruct.drive_state) {
        pumpStatusStruct.drive_state = statusMsg[STAT_STATE_IDX];
        if (ISDEBUG) statOutput += "\tSTE: " + String(statusMsg[STAT_STATE_IDX]) + "\n";
        if (aflib->setAttribute8(AF_STATUS__PUMP_DRIVE_STATE, pumpStatusStruct.drive_state) != afSUCCESS) {
            if (ISDEBUG) Serial.println("Couldn't set drive state attribute");
        }
    }

    uint16_t pwr_usage = (statusMsg[STAT_PWR_HB_IDX] * 256) + statusMsg[STAT_PWR_LB_IDX];
    if (pwr_usage != pumpStatusStruct.pwr_usage) {
        pumpStatusStruct.pwr_usage = pwr_usage;
        if (ISDEBUG) statOutput += "\tPWR: " + String(pwr_usage) + " WATT" + "\n";
        if (aflib->setAttribute16(AF_STATUS__PUMP_POWER_USAGE__W_, pumpStatusStruct.pwr_usage) != afSUCCESS) {
            if (ISDEBUG) Serial.println("Couldn't set power usage attribute");
        }
    }

    uint16_t speed = (statusMsg[STAT_RPM_HB_IDX] * 256) + statusMsg[STAT_RPM_LB_IDX];
    if (speed != pumpStatusStruct.speed) {
        pumpStatusStruct.speed = speed;
        if (ISDEBUG) statOutput += "\tRPM: " + String(speed) + " RPM" + "\n";
        if (aflib->setAttribute16(AF_STATUS__PUMP_SPEED__RPM_, pumpStatusStruct.speed) != afSUCCESS) {
            if (ISDEBUG) Serial.println("Couldn't set speed attribute");
        }
    }

    if (statusMsg[STAT_GPM_IDX] != pumpStatusStruct.flow_rate) {
        pumpStatusStruct.flow_rate = statusMsg[STAT_GPM_IDX];
        if (ISDEBUG) statOutput += "\tGPM: " + String(statusMsg[STAT_GPM_IDX]) + " GPM" + "\n";
        if (aflib->setAttribute8(AF_STATUS__PUMP_FLOW_RATE__GPM_, pumpStatusStruct.flow_rate) != afSUCCESS) {
            if (ISDEBUG) Serial.println("Couldn't set flow rate attribute");
        }
    }

    if (statusMsg[STAT_PPC_IDX] != pumpStatusStruct.ppc_levels) {
        pumpStatusStruct.ppc_levels = statusMsg[STAT_PPC_IDX];
        if (ISDEBUG) statOutput += "\tPPC: " + String(statusMsg[STAT_PPC_IDX]) + " %" + "\n";
        if (aflib->setAttribute8(AF_STATUS__PUMP_PPC_LEVELS____, pumpStatusStruct.speed) != afSUCCESS) {
            if (ISDEBUG) Serial.println("Couldn't set PPC levels attribute");
        }
    }

    if (statusMsg[STAT_B09_IDX] != pumpStatusStruct.b09) {
        pumpStatusStruct.b09 = statusMsg[STAT_B09_IDX];
        if (ISDEBUG) statOutput += "\tB09: " + String(statusMsg[STAT_B09_IDX]) + "\n";
        if (aflib->setAttribute8(AF_STATUS__PUMP_BYTE_09____, pumpStatusStruct.b09) != afSUCCESS) {
            if (ISDEBUG) Serial.println("Couldn't set byte 09 attribute");
        }
    }

    if (statusMsg[STAT_ERR_IDX] != pumpStatusStruct.error_code) {
        pumpStatusStruct.error_code = statusMsg[STAT_ERR_IDX];
        if (ISDEBUG) statOutput += "\tERR: " + String(statusMsg[STAT_ERR_IDX]) + "\n";
        if (aflib->setAttribute8(AF_STATUS__PUMP_ERROR_CODE, pumpStatusStruct.error_code) != afSUCCESS) {
            if (ISDEBUG) Serial.println("Couldn't set error code attribute");
        }
    }

    if (statusMsg[STAT_B11_IDX] != pumpStatusStruct.b11) {
        pumpStatusStruct.b11 = statusMsg[STAT_B11_IDX];
        if (ISDEBUG) statOutput += "\tB11: " + String(statusMsg[STAT_B11_IDX]) + "\n";
        if (aflib->setAttribute8(AF_STATUS__PUMP_BYTE_11____, pumpStatusStruct.b11) != afSUCCESS) {
            if (ISDEBUG) Serial.println("Couldn't set byte 11 attribute");
        }
    }

    if (statusMsg[STAT_TIMER_IDX] != pumpStatusStruct.timer) {
        pumpStatusStruct.timer = statusMsg[STAT_TIMER_IDX];
        if (ISDEBUG) statOutput += "\tTMR: " + String(statusMsg[STAT_TIMER_IDX]) + " MIN" + "\n";
        if (aflib->setAttribute16(AF_STATUS__PUMP_TIMER__MIN_, pumpStatusStruct.timer) != afSUCCESS) {
            if (ISDEBUG) Serial.println("Couldn't set timer attribute");
        }
    }

    char clk[6] = "--:--";
    clk[0] = '0' + statusMsg[STAT_CLK_HOUR_IDX] / 10;
    clk[1] = '0' + statusMsg[STAT_CLK_HOUR_IDX] % 10;
    clk[3] = '0' + statusMsg[STAT_CLK_MIN_IDX] / 10;
    clk[4] = '0' + statusMsg[STAT_CLK_MIN_IDX] % 10;

    if (strcmp(clk, pumpStatusStruct.clock) != 0) {
        strcpy(pumpStatusStruct.clock, clk);
        if (ISDEBUG) statOutput += "\tCLK: " + String(clk) + "\n";
        if (aflib->setAttribute(AF_STATUS__PUMP_CLOCK__HH_MM_, AF_STATUS__PUMP_CLOCK__HH_MM__SZ, pumpStatusStruct.clock) != afSUCCESS) {
            if (ISDEBUG) Serial.println("Couldn't set clock attribute");
        }
    }

    if (ISDEBUG) {
        Serial.print(statOutput);
    }
}

void reset() {
    // Stop the timer
    timer1.stop(commandTtlTimerId);

    memset(msgBuffer, 0, sizeof(msgBuffer));
    msgBufPtr = msgBuffer;
    commandQueue->Clear();
    commandStage = CMD_STAGE_IDLE;
    commandTtlTimerId = -1;
}

/*
 * Callback to query pump status every 15 seconds, if we are idle
 */
void queryStatusCb() {
    Serial.println("Status query callback executing... commandStage: " + String(commandStage));
    uint8_t c = CMD_STATUS;
    queuePumpInstruction(&c);
}

/*
 * Callback that will reset the message buffers, if we haven't received a complete response from the pump within a given timewindow
 */
void commandTimeoutCb() {
    Serial.println("Command timeout callback executing... commandStage: " + String(commandStage) + ", message buffer size: " + String((msgBufPtr - msgBuffer)));

    if (commandStage != CMD_STAGE_IDLE) {
        reset();
    }
}

//region afLib integration
/*
 * Define this wrapper to allow the instance method to be called
 * when the interrupt fires. We do this because attachInterrupt
 * requires a method with no parameters and the instance method
 * has an invisible parameter (this).
 */
void ISRWrapper() {
    if (aflib) {
        aflib->mcuISR();
    }
}

/*
 * This is called when the service changes one of our attributes.
 */
void onAttrSet(const uint8_t requestId, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
    switch (attributeId) {
        // This MCU attribute tells us whether we should be blinking.
        case AF_COMMAND:
            queuePumpInstruction(value);
            break;
        case AF_SET_PUMP_ADDRESS:
            pumpAddress = *value;
            break;
        case AF_SET_CONTROLLER_ADDRESS:
            controllerAddress = *value;
            break;
        default:
            break;
    }

    if (aflib->setAttributeComplete(requestId, attributeId, valueLen, value) != afSUCCESS) {
        Serial.println("setAttributeComplete failed!");
    }
}

/*
 * This is called when either an Afero attribute has been changed via setAttribute or in response
 * to a getAttribute call.
 */
void onAttrSetComplete(const uint8_t requestId, const uint16_t attributeId, const uint16_t valueLen, const uint8_t *value) {
    // This is a noop in our case as there is nothing for us to do if an attribute is setb
    if (ISDEBUG) Serial.println("onAttrSetComplete: " + String(attributeId) + ":" + String(*value));
}
//endregion afLib integration
