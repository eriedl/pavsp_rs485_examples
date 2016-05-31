/* This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://www.wtfpl.net/ for more details.
 */

#include <Arduino.h>
#include <SoftwareSerial.h>

#define SSerialTx        8 // DI: data in
#define SSerialTxControl 9 // DE: data enable
#define RE_PIN           10 // RE: receive enable; jumpered together with 6, will be put into floating mode
#define SSerialRx        11 // RO: receive out

#define RS485Transmit    HIGH
#define RS485Receive     LOW
#define Pin13LED         13

boolean isDataValid(int *data, int len);

// The theoretical maximum is 264 bytes. however, it seems that actual message are much shorter, i.e. 37 bytes was
// the longest seen so far. So, allocate 2x size of an expected message size: 2 x 32 bytes.
//
//#define MAX_PKG_LEN      6+256+2 //Msg Prefix + Data + Checksum
#define MAX_PKG_LEN      6 + 32 + 2 // 40 bytes
#define MIN_PKG_LEN      6 + 0 + 2 // 8 bytes
#define DATA_LENGTH_IDX  5
#define RCV_BUF_SZ       (3 + MAX_PKG_LEN) * 2 // 86 bytes, includes preamble
const uint8_t PREAMBLE[] = {0xFF, 0x00, 0xFF, 0xA5};
int lastBufferIdx = 0;
byte receiveBuffer[RCV_BUF_SZ] = {};

SoftwareSerial RS485Serial(SSerialRx, SSerialTx); // RX, TX

void setup() {
    Serial.begin(9600);
    Serial.println("RS485 Remote Echo...");

    pinMode(SSerialTxControl, OUTPUT);

    pinMode(Pin13LED, OUTPUT);

    // Init Transceiver, we start with listening
    digitalWrite(SSerialTxControl, RS485Receive);

    // Start the software serial port, to another device
    RS485Serial.begin(9600);   // set the data rate
}

void loop() {
    digitalWrite(Pin13LED, HIGH);   // turn LED on

    if (RS485Serial.available() > 0) {
        digitalWrite(Pin13LED, LOW);

        Serial.println("Receiving data...");

        int iRcv = -1;
        while ((iRcv = RS485Serial.read()) > -1) {
            receiveBuffer[lastBufferIdx++] = (byte)iRcv;

            Serial.print(iRcv < 0x10 ? "0" + String(iRcv, HEX) : String(iRcv, HEX));

            // Reset the buffer
            if (lastBufferIdx > RCV_BUF_SZ) {
                lastBufferIdx = 0;
            }
        }

        Serial.println("\nThe End.");
    }

    // Check if we have a valid message and then echo it back
    if (lastBufferIdx >= MIN_PKG_LEN) {
        int msgStartIdx = -1;
        int msgLength = -1;
        boolean msgFound = findPAMessage(receiveBuffer, lastBufferIdx, &msgStartIdx, &msgLength);
        Serial.println("findPAMessage results: \nlastBufferIdx: " + String(lastBufferIdx)
                       + "\nmsgFound: " + String(msgFound)
                       + "\nmsgStartIdx: " + String(msgStartIdx)
                       + "\nmsgLength: " + String(msgLength));

        // Echo the data received
        if (msgFound == true) {
            digitalWrite(Pin13LED, HIGH);
            digitalWrite(SSerialTxControl, RS485Transmit);

            uint8_t statResp[] = {0xff, 0x00, 0xff, 0xa5, 0x00, 0x11, 0x60, 0x07, 0x0f, 0x0a, 0x00, 0x00, 0x01, 0xe7, 0x07, 0x9e, 0x00, 0x00, 0x00, 0x00, 0x18, 0x01, 0x10, 0x26, 0x03, 0x12};

            // Send fake status back
            if (receiveBuffer[msgStartIdx + 4] == 7) {
                Serial.println("Sending fake status: ");
                statResp[5] = receiveBuffer[msgStartIdx + 3];
                statResp[6] = receiveBuffer[msgStartIdx + 2];

                int chkSum = 0;
                for (int i = 3; i < 24; ++i) {
                    chkSum += statResp[i];
                }
                statResp[24] = chkSum >> 8;
                statResp[25] = chkSum & 0xFF;

                for (int i = 0; i < sizeof(statResp); ++i) {
                    Serial.print(statResp[i] < 0x10 ? "0" + String(statResp[i], HEX) : String(statResp[i], HEX));
                }
                Serial.println();

                RS485Serial.write(statResp, sizeof(statResp));
            } else {
                Serial.println("Echoing message with flipped src/dst: ");
                //Flip source/dest
                uint8_t dst = receiveBuffer[msgStartIdx + 2];
                receiveBuffer[msgStartIdx + 2] = receiveBuffer[msgStartIdx + 3];
                receiveBuffer[msgStartIdx + 3] = dst;

                for (int i = 0; i < sizeof(PREAMBLE); ++i) {
                    Serial.print(PREAMBLE[i] < 0x10 ? "0" + String(PREAMBLE[i], HEX) : String(PREAMBLE[i], HEX));
                }

                for (int i = msgStartIdx + 1; i < msgStartIdx + msgLength; ++i) {
                    Serial.print(receiveBuffer[i] < 0x10 ? "0" + String(receiveBuffer[i], HEX) : String(receiveBuffer[i], HEX));
                }
                Serial.println();

                RS485Serial.write(PREAMBLE, sizeof(PREAMBLE));
                RS485Serial.write(&receiveBuffer[msgStartIdx + 1], msgLength - 1); // Skip the A5 byte
            }

            digitalWrite(SSerialTxControl, RS485Receive);
            digitalWrite(Pin13LED, LOW);

            // Reset the buffer
            lastBufferIdx = 0;
        } else {
            // We found the start of a message but either the message was incomplete or the checksum failed
            if (msgStartIdx > -1 || msgLength == 0) {
                lastBufferIdx = 0;
            }
        }
    }
}

boolean findPAMessage(uint8_t data[], int len, int *msgStartIdx, int *actualMsgLength) {
    *msgStartIdx = -1; // the starting index of a valid message, if any
    *actualMsgLength = 0;

    // Find the start of the message
    for (int i = 0; i < (len - (sizeof(PREAMBLE) / sizeof(uint8_t))); ++i) {
        if (data[i] == PREAMBLE[0]
            && data[i + 1] == PREAMBLE[1]
            && data[i + 2] == PREAMBLE[2]
            && data[i + 3] == PREAMBLE[3]) {
            *msgStartIdx = i + 3;
            break;
        }
    }

    // Check if the found start index + minimum package length is within total length of message
    if (*msgStartIdx == -1 || (*msgStartIdx + MIN_PKG_LEN) > len) {
        return false;
    }

    // Get the actual payload length. It's the fifth byte in the message
    int dataLength = data[*msgStartIdx + DATA_LENGTH_IDX];

    // Check again if total message length is within the boundaries given the data length
    *actualMsgLength = (DATA_LENGTH_IDX + 1) + dataLength + 2; // Index + 1, actual data length, check sum
    if (*msgStartIdx + *actualMsgLength > len) {
        return false;
    }

    // Calculate the checksum
    int calcChkSum = 0;
    int msgChkSum = (data[*msgStartIdx + *actualMsgLength - 2] * 256) + data[*msgStartIdx + *actualMsgLength - 1];
    for (int i = *msgStartIdx; i < (*msgStartIdx + *actualMsgLength - 2); ++i) {
        calcChkSum += data[i];
    }

    Serial.println("Calc checksum: " + String(calcChkSum));
    Serial.println("High byte: " + String(data[*msgStartIdx + *actualMsgLength - 2]));
    Serial.println("Low byte: " + String(data[*msgStartIdx + *actualMsgLength - 1]));
    Serial.println("Msg checksum: " + String(msgChkSum));

    // Verify the checksum
    if (calcChkSum != msgChkSum) {
        Serial.println("Checksum result: FAILED");

        return false;
    }

    Serial.println("Checksum result: GOOD");
    return true;
}
