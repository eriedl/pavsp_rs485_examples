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
#include "rs485_master.h"
#include "profile/device-description.h"
#include "CommandQueue.h"

iafLib *aflib;

//region Serial command input
#define MAX_INPUT_LEN       16
uint8_t inputCmdBuffer[MAX_INPUT_LEN];
uint8_t *inputCmdBufPtr;
//endregion Serial command input

//region RS485 message processing
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
COMMAND_STAGE commandStage;
CommandQueue commandQueue;
PumpStatus pumpStatusStruct;

uint8_t pumpAddress;
uint8_t controllerAddress;
COMMAND lastCommand;

#define STATUS_QRY_TIMEOUT  1000 * 15   // 15 seconds
#define COMMAND_TIMEOUT     1000 * 2    // 2 second
Timer timer1;
int8_t statusQryTimerId;
int8_t commandTtlTimerId;
//endregion State machine

String strSerialCommand;
boolean ISDEBUG             = false;

void setup() {
    Serial.begin(9600);
    while (!Serial) {
        ;
    }

    Serial.println("Arduino Pentair Pool Pump Controller...");

#if defined(AFLIB_IN_USE)
    //region Arduino Board setup
    // The Plinto board automatically connects reset on UNO to reset on Modulo
    // For Teensy, we need to reset manually...
#if defined(TEENSYDUINO)
    Serial.println("Using Teensy - Resetting Modulo");
    pinMode(RESET, OUTPUT);
    digitalWrite(RESET, 0);
    delay(250);
    digitalWrite(RESET, 1);
#elif defined(ARDUINO_AVR_MEGA2560)
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
#endif

#if defined(RUN_PROGRAM)
    //region RS485 setup
    pinMode(SSerialTxControl, OUTPUT);

    // Init Transceiver, we start with listening
    digitalWrite(SSerialTxControl, RS485Receive);
    // Start the software serial port, to another device
    RS485Serial.begin(9600);   // set the data rate
    //endregion

    // Initialize gloabl program variables
    commandQueue = CommandQueue();
    timer1 = Timer();
    statusQryTimerId = -1;
    commandTtlTimerId = -1;
    commandStage = CMD_STAGE_IDLE;
    memset(msgBuffer, 0, sizeof(msgBuffer));
    msgBufPtr = msgBuffer;
    lastCommand = CMD_NOOP;

    // Initialize serial input buffer
    memset(inputCmdBuffer, 0, sizeof(inputCmdBuffer));
    inputCmdBufPtr = inputCmdBuffer;

    /*
     * Every device on the bus has an address:
     *      0x0f - is the broadcast address, it is used by the more sophisticated controllers as <dst>
     *          in their system status broadcasts most likely to keep queries for system status low.
     *      0x1x - main controllers (IntelliComII, IntelliTouch, EasyTouch ...)
     *      0x2x - remote controllers
     *      0x6x - pumps, 0x60 is pump 1
     *
     * Let's use 0x20, the first address in the remote controller space
     */
    pumpAddress = 0x60;
    controllerAddress = 0x20;

    // Initialize pump status
    pumpStatusStruct.ctrl_mode = CTRL_MODE_LOCAL;
    pumpStatusStruct.pumpAddr = pumpAddress;
    pumpStatusStruct.running = IFLO_RUN_STOP;
    pumpStatusStruct.mode = IFLO_MODE_FILTER;

    // Start a status query timer
//    statusQryTimerId = timer1.every(STATUS_QRY_TIMEOUT, queryStatusCb);
    if (ISDEBUG) Serial.println("statusQryTimerId: " + String(statusQryTimerId));
#endif
}

void loop() {
#if defined(RUN_PROGRAM)
    //region Serial command input
    // Parse a command
    if (Serial.available()) {
        int iRcv = -1;

        while ((iRcv = Serial.read()) > -1) {
            // Line ending indicates end of command
            if (iRcv == 0x0A || iRcv == 0x0D) {
                strSerialCommand = String((char *)inputCmdBuffer);
                uint8_t c = strSerialCommand.toInt();
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

        Serial.println("Queue size: " + String(commandQueue.GetSize()));

        const CommandStruct *cmd = commandQueue.Peek();

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
            const CommandStruct *commandStructPtr = commandQueue.Peek();
            uint8_t *relMsgPtr = &msgBuffer[msgStartIdx];
            // Reset the buffer and bail if the message was not for us and it is not a broadcast
            if (
//                    (showBroadcasts == true && relMsgPtr[MSG_DST_IDX] != BROADCAST_ADDRESS) &&
                    (relMsgPtr[MSG_DST_IDX] != controllerAddress || relMsgPtr[MSG_SRC_IDX] != pumpAddress)) {
                if (ISDEBUG) Serial.println("Message not for us SRC: " + String(relMsgPtr[MSG_DST_IDX], HEX) + " DST: " + String(relMsgPtr[MSG_SRC_IDX], HEX));
                memset(msgBuffer, 0, sizeof(msgBuffer));
                msgBufPtr = msgBuffer;

                return;
            }

            // Response is not for our last command
            if (relMsgPtr[MSG_CFI_IDX] != commandStructPtr->command[MSG_CFI_IDX]) {
                if (ISDEBUG) Serial.println("Message is not a confirmation of our last command: SENT: "
                                            + String(commandStructPtr->command[MSG_CFI_IDX], HEX) + " RCVD: " + String(relMsgPtr[MSG_CFI_IDX], HEX));
                reset();

                return;
            }

            // Response should also contain the last command value, unless it was a status request
            if (commandStructPtr->command[MSG_CFI_IDX] == IFLO_CMD_STAT) {
                // TODO: Queue attribute updates
                figureOutChangedAttributes(relMsgPtr);
            } else {
                for (int i = MSG_LEN_IDX + 1; i < relMsgPtr[MSG_LEN_IDX]; ++i) {
                    uint8_t lCmdVal = 0x00;

                    // Register writes echo only the value!
                    if (commandStructPtr->command[MSG_CFI_IDX] == IFLO_CMD_REG) {
                        lCmdVal = commandStructPtr->command[i + sizeof(IFLO_REG_EPRG)];
                    } else {
                        lCmdVal = commandStructPtr->command[i];
                    }

                    if (relMsgPtr[i] != lCmdVal) {
                        if (ISDEBUG)Serial.println("Message does not contain expected value of sent command value at data index "
                                                   + String(i) + " SENT: " + String(lCmdVal) + " RCVD: " + String(relMsgPtr[i]));
                        reset();

                        return;
                    }
                }
            }

            if (ISDEBUG) Serial.println("Last command confirmed");

            // Reset the receiving buffer and dispose of the last command
            memset(msgBuffer, 0, sizeof(msgBuffer));
            msgBufPtr = msgBuffer;
            timer1.stop(commandTtlTimerId);
            commandTtlTimerId = -1;
            CommandStruct *tmpCmdStruct = commandQueue.Dequeue();
            free(tmpCmdStruct->command);
            free(tmpCmdStruct);

            // We should be good now
            if (commandQueue.HasNext() == true) {
                commandStage = CMD_STAGE_SEND;
            } else {
                commandStage = CMD_STAGE_IDLE;
                Serial.println("lastCommand: " + String(lastCommand));
                // Reset the Command attribute to previous command now that the chain has completed
                if (lastCommand >= CMD_RUN_PROG_1 && lastCommand <= CMD_RUN_PROG_4) {
                    Serial.println("resetting lastCommand: " + String(lastCommand));
                    aflib->setAttribute8(AF_PUMP_COMMAND, lastCommand);
                }
            }
        }
    }
    //endregion

    timer1.update();
#endif

#if defined(AFLIB_IN_USE)
    aflib->loop();
#endif
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
    CommandStruct *commandStruct;

    Serial.println("Queueing pump instruction: " + String(*instruction));

    if (commandStage == CMD_STAGE_IDLE && *instruction > CMD_NOOP) {
        // Fill the command queue
        //1. Set the pump into remote control mode, if it's not in remote control mode yet
        if (pumpStatusStruct.ctrl_mode == CTRL_MODE_LOCAL && *instruction != CMD_CTRL_REMOTE && *instruction != CMD_CTRL_LOCAL) {
            commandStruct = buildCommandStruct(cmdArrCtrlRemote, sizeof(cmdArrCtrlRemote));
            commandQueue.Enqueue(commandStruct);
        }

        switch(*instruction) {
            case 127:
                ISDEBUG = (ISDEBUG == true ? false : true);
                Serial.println("Command: Set ISDEBUG to " + String(ISDEBUG));
                return;
            case CMD_ALL_OFF:
                Serial.println("Command: Turning external programs off");
                commandStruct = buildCommandStruct(cmdArrExtProgOff, sizeof(cmdArrExtProgOff));
                commandQueue.Enqueue(commandStruct);
                break;
            case CMD_RUN_PROG_1:
                Serial.println("Command: Running external program 1");
                commandStruct = buildCommandStruct(cmdArrRunExtProg1, sizeof(cmdArrRunExtProg1));
                commandQueue.Enqueue(commandStruct);

                // TODO: Verify that we have to send the RUN command everytime we change the program
                // Queue the start command, if the pump is not running yet
                if (pumpStatusStruct.running == IFLO_RUN_STOP) {
                    commandStruct = buildCommandStruct(cmdArrStartPump, sizeof(cmdArrStartPump));
                    commandQueue.Enqueue(commandStruct);
                }

                lastCommand = CMD_RUN_PROG_1;
                break;
            case CMD_RUN_PROG_2:
                Serial.println("Command: Running external program 2");
                commandStruct = buildCommandStruct(cmdArrRunExtProg2, sizeof(cmdArrRunExtProg2));
                commandQueue.Enqueue(commandStruct);

                // TODO: Verify that we have to send the RUN command everytime we change the program
                // Queue the start command, if the pump is not running yet
                if (pumpStatusStruct.running == IFLO_RUN_STOP) {
                    commandStruct = buildCommandStruct(cmdArrStartPump, sizeof(cmdArrStartPump));
                    commandQueue.Enqueue(commandStruct);
                }

                lastCommand = CMD_RUN_PROG_2;
                break;
            case CMD_RUN_PROG_3:
                Serial.println("Command: Running external program 3");
                commandStruct = buildCommandStruct(cmdArrRunExtProg3, sizeof(cmdArrRunExtProg3));
                commandQueue.Enqueue(commandStruct);

                // TODO: Verify that we have to send the RUN command everytime we change the program
                // Queue the start command, if the pump is not running yet
                if (pumpStatusStruct.running == IFLO_RUN_STOP) {
                    commandStruct = buildCommandStruct(cmdArrStartPump, sizeof(cmdArrStartPump));
                    commandQueue.Enqueue(commandStruct);
                }

                lastCommand = CMD_RUN_PROG_3;
                break;
            case CMD_RUN_PROG_4:
                Serial.println("Command: Running external program 4");
                commandStruct = buildCommandStruct(cmdArrRunExtProg4, sizeof(cmdArrRunExtProg4));
                commandQueue.Enqueue(commandStruct);

                // TODO: Verify that we have to send the RUN command everytime we change the program
                // Queue the start command, if the pump is not running yet
                if (pumpStatusStruct.running == IFLO_RUN_STOP) {
                    commandStruct = buildCommandStruct(cmdArrStartPump, sizeof(cmdArrStartPump));
                    commandQueue.Enqueue(commandStruct);
                }

                lastCommand = CMD_RUN_PROG_4;
                break;
            case CMD_STATUS:
                Serial.println("Command: Querying pump status");
                commandStruct = buildCommandStruct(cmdArrGetStatus, sizeof(cmdArrGetStatus));
                commandQueue.Enqueue(commandStruct);
                break;
            case CMD_CTRL_REMOTE:
                Serial.println("Command: Setting control remote");
                commandStruct = buildCommandStruct(cmdArrCtrlRemote, sizeof(cmdArrCtrlRemote));
                commandQueue.Enqueue(commandStruct);
                pumpStatusStruct.ctrl_mode = CTRL_MODE_REMOTE;
                break;
            case CMD_CTRL_LOCAL:
                Serial.println("Command: Setting control local");
                commandStruct = buildCommandStruct(cmdArrCtrlLocal, sizeof(cmdArrCtrlLocal));
                commandQueue.Enqueue(commandStruct);
                pumpStatusStruct.ctrl_mode = CTRL_MODE_LOCAL;
                break;
//            case CMD_PUMP_ON:
//                Serial.println("Command: Turning pump on");
//                commandStruct = buildCommandStruct(cmdArrStartPump, sizeof(cmdArrStartPump));
//                commandQueue.Enqueue(commandStruct);
//                break;
            case CMD_PUMP_OFF:
                Serial.println("Command: Turning pump off");
                commandStruct = buildCommandStruct(cmdArrStopPump, sizeof(cmdArrStopPump));
                commandQueue.Enqueue(commandStruct);
                break;
            default:
                Serial.print("What? Resetting....");
                reset();

                return;
        }

        // Set the pump into local control mode, if it has not been set into explicit remote control mode
        if (pumpStatusStruct.ctrl_mode == CTRL_MODE_LOCAL && *instruction != CMD_CTRL_REMOTE && *instruction != CMD_CTRL_LOCAL) {
            commandStruct = buildCommandStruct(cmdArrCtrlLocal, sizeof(cmdArrCtrlLocal));
            commandQueue.Enqueue(commandStruct);
        }

        commandStage = CMD_STAGE_SEND;
    }

    Serial.println("queuePumpInstructions lastCommand: " + String(lastCommand));
}

CommandStruct *buildCommandStruct(uint8_t *command, size_t size) {
    uint16_t chkSum = 0;
    CommandStruct *commandStruct = (CommandStruct *)malloc(sizeof(CommandStruct));

    commandStruct->command = (uint8_t *)malloc(size);
    commandStruct->size = size;

    Serial.println("Queueing command");
    Serial.print("Data: ");
    for (int i = 0; i < size; ++i) {
        commandStruct->command[i] = command[i];

        if (i == MSG_DST_IDX) {
            commandStruct->command[i] = pumpAddress;
        } else if (i == MSG_SRC_IDX) {
            commandStruct->command[i] = controllerAddress;
        }

        if (i >= MSG_BGN_IDX && i < size - 2) {
            chkSum += commandStruct->command[i]; // Calculate the checksum
        }

        if (i == size - 2) {
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

    if (pumpAddress != pumpStatusStruct.pumpAddr) {
        pumpStatusStruct.pumpAddr = pumpAddress;
#if defined(AFLIB_IN_USE)
        if (aflib->setAttribute8(AF_STATUS__CURRENT_PUMP_ADDRESS, pumpStatusStruct.pumpAddr) != afSUCCESS) {
            if (ISDEBUG) Serial.println("Couldn't set current pump address attribute");
        }
#endif
    }

    if (statusMsg[STAT_RUN_IDX] != pumpStatusStruct.running) {
        pumpStatusStruct.running = statusMsg[STAT_RUN_IDX];
        if (ISDEBUG) statOutput += "\tRUN: " + String(statusMsg[STAT_RUN_IDX]) + "\n";
#if defined(AFLIB_IN_USE)
        if (aflib->setAttribute8(AF_STATUS__PUMP_RUNNING_STATE, pumpStatusStruct.running) != afSUCCESS) {
            if (ISDEBUG) Serial.println("Couldn't set running status attribute");
        }
#endif
    }

    if (statusMsg[STAT_MODE_IDX] != pumpStatusStruct.mode) {
        pumpStatusStruct.mode = statusMsg[STAT_MODE_IDX];
        if (ISDEBUG) statOutput += "\tMOD: " + String(statusMsg[STAT_MODE_IDX]) + "\n";
#if defined(AFLIB_IN_USE)
        if (aflib->setAttribute8(AF_STATUS__PUMP_MODE, pumpStatusStruct.mode) != afSUCCESS) {
            if (ISDEBUG) Serial.println("Couldn't set mode attribute");
        }
#endif
    }

    if (statusMsg[STAT_STATE_IDX] != pumpStatusStruct.drive_state) {
        pumpStatusStruct.drive_state = statusMsg[STAT_STATE_IDX];
        if (ISDEBUG) statOutput += "\tSTE: " + String(statusMsg[STAT_STATE_IDX]) + "\n";
#if defined(AFLIB_IN_USE)
        if (aflib->setAttribute8(AF_STATUS__PUMP_DRIVE_STATE, pumpStatusStruct.drive_state) != afSUCCESS) {
            if (ISDEBUG) Serial.println("Couldn't set drive state attribute");
        }
#endif
    }

    uint16_t pwr_usage = (statusMsg[STAT_PWR_HB_IDX] * 256) + statusMsg[STAT_PWR_LB_IDX];
    if (pwr_usage != pumpStatusStruct.pwr_usage) {
        pumpStatusStruct.pwr_usage = pwr_usage;
        if (ISDEBUG) statOutput += "\tPWR: " + String(pwr_usage) + " WATT" + "\n";
#if defined(AFLIB_IN_USE)
        if (aflib->setAttribute16(AF_STATUS__PUMP_POWER_USAGE__W_, pumpStatusStruct.pwr_usage) != afSUCCESS) {
            if (ISDEBUG) Serial.println("Couldn't set power usage attribute");
        }
#endif
    }

    uint16_t speed = (statusMsg[STAT_RPM_HB_IDX] * 256) + statusMsg[STAT_RPM_LB_IDX];
    if (speed != pumpStatusStruct.speed) {
        pumpStatusStruct.speed = speed;
        if (ISDEBUG) statOutput += "\tRPM: " + String(speed) + " RPM" + "\n";
#if defined(AFLIB_IN_USE)
        if (aflib->setAttribute16(AF_STATUS__PUMP_SPEED__RPM_, pumpStatusStruct.speed) != afSUCCESS) {
            if (ISDEBUG) Serial.println("Couldn't set speed attribute");
        }
#endif
    }

    if (statusMsg[STAT_GPM_IDX] != pumpStatusStruct.flow_rate) {
        pumpStatusStruct.flow_rate = statusMsg[STAT_GPM_IDX];
        if (ISDEBUG) statOutput += "\tGPM: " + String(statusMsg[STAT_GPM_IDX]) + " GPM" + "\n";
#if defined(AFLIB_IN_USE)
        if (aflib->setAttribute8(AF_STATUS__PUMP_FLOW_RATE__GPM_, pumpStatusStruct.flow_rate) != afSUCCESS) {
            if (ISDEBUG) Serial.println("Couldn't set flow rate attribute");
        }
#endif
    }

    if (statusMsg[STAT_PPC_IDX] != pumpStatusStruct.ppc_levels) {
        pumpStatusStruct.ppc_levels = statusMsg[STAT_PPC_IDX];
        if (ISDEBUG) statOutput += "\tPPC: " + String(statusMsg[STAT_PPC_IDX]) + " %" + "\n";
#if defined(AFLIB_IN_USE)
        if (aflib->setAttribute8(AF_STATUS__PUMP_PPC_LEVELS____, pumpStatusStruct.speed) != afSUCCESS) {
            if (ISDEBUG) Serial.println("Couldn't set PPC levels attribute");
        }
#endif
    }

    if (statusMsg[STAT_B09_IDX] != pumpStatusStruct.b09) {
        pumpStatusStruct.b09 = statusMsg[STAT_B09_IDX];
        if (ISDEBUG) statOutput += "\tB09: " + String(statusMsg[STAT_B09_IDX]) + "\n";
#if defined(AFLIB_IN_USE)
        if (aflib->setAttribute8(AF_STATUS__PUMP_BYTE_09____, pumpStatusStruct.b09) != afSUCCESS) {
            if (ISDEBUG) Serial.println("Couldn't set byte 09 attribute");
        }
#endif
    }

    if (statusMsg[STAT_ERR_IDX] != pumpStatusStruct.error_code) {
        pumpStatusStruct.error_code = statusMsg[STAT_ERR_IDX];
        if (ISDEBUG) statOutput += "\tERR: " + String(statusMsg[STAT_ERR_IDX]) + "\n";
#if defined(AFLIB_IN_USE)
        if (aflib->setAttribute8(AF_STATUS__PUMP_ERROR_CODE, pumpStatusStruct.error_code) != afSUCCESS) {
            if (ISDEBUG) Serial.println("Couldn't set error code attribute");
        }
#endif
    }

    if (statusMsg[STAT_B11_IDX] != pumpStatusStruct.b11) {
        pumpStatusStruct.b11 = statusMsg[STAT_B11_IDX];
        if (ISDEBUG) statOutput += "\tB11: " + String(statusMsg[STAT_B11_IDX]) + "\n";
#if defined(AFLIB_IN_USE)
        if (aflib->setAttribute8(AF_STATUS__PUMP_BYTE_11____, pumpStatusStruct.b11) != afSUCCESS) {
            if (ISDEBUG) Serial.println("Couldn't set byte 11 attribute");
        }
#endif
    }

    if (statusMsg[STAT_TIMER_IDX] != pumpStatusStruct.timer) {
        pumpStatusStruct.timer = statusMsg[STAT_TIMER_IDX];
        if (ISDEBUG) statOutput += "\tTMR: " + String(statusMsg[STAT_TIMER_IDX]) + " MIN" + "\n";
#if defined(AFLIB_IN_USE)
        if (aflib->setAttribute16(AF_STATUS__PUMP_TIMER__MIN_, pumpStatusStruct.timer) != afSUCCESS) {
            if (ISDEBUG) Serial.println("Couldn't set timer attribute");
        }
#endif
    }

    char clk[6] = "--:--";
    clk[0] = '0' + statusMsg[STAT_CLK_HOUR_IDX] / 10;
    clk[1] = '0' + statusMsg[STAT_CLK_HOUR_IDX] % 10;
    clk[3] = '0' + statusMsg[STAT_CLK_MIN_IDX] / 10;
    clk[4] = '0' + statusMsg[STAT_CLK_MIN_IDX] % 10;

    if (strcmp(clk, pumpStatusStruct.clock) != 0) {
        strcpy(pumpStatusStruct.clock, clk);
        if (ISDEBUG) statOutput += "\tCLK: " + String(clk) + "\n";
#if defined(AFLIB_IN_USE)
        if (aflib->setAttribute(AF_STATUS__PUMP_CLOCK__HH_MM_, AF_STATUS__PUMP_CLOCK__HH_MM__SZ, pumpStatusStruct.clock) != afSUCCESS) {
            if (ISDEBUG) Serial.println("Couldn't set clock attribute");
        }
#endif
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
    commandQueue.Clear();
    commandStage = CMD_STAGE_IDLE;
    commandTtlTimerId = -1;
    lastCommand = CMD_NOOP;
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

#if defined(AFLIB_IN_USE)
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
    Serial.println("Got attribute update: " + String(attributeId) + ", " + String(*value));

    switch (attributeId) {
        case AF_PUMP_COMMAND:
            queuePumpInstruction(value);
            break;
        case AF_SET_PUMP_ADDRESS:
            pumpAddress = *value;
            break;
        case AF_SET_CONTROLLER_ADDRESS:
            controllerAddress = *value;
            break;
        default:
            Serial.println("Attribute ID not handled: " + String(attributeId));
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
    Serial.println("onAttrSetComplete: " + String(attributeId) + ":" + String(*value));
}
//endregion afLib integration
#endif