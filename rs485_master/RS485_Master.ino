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
#define MY_ADDRESS          0x20
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

#define MSG_VER_IDX         0x04
#define MSG_DST_IDX         0x05
#define MSG_SRC_IDX         0x06
#define MSG_CFI_IDX         0x07
#define MSG_LEN_IDX         0x08

// Pump status structure indexes
#define STAT_RUN_IDX        0x00
#define STAT_MODE_IDX       0x01
#define STAT_STATE_IDX      0x02
#define STAT_PWR_HB_IDX     0x03
#define STAT_PWR_LB_IDX     0x04
#define STAT_RPM_HB_IDX     0x05
#define STAT_RPM_LB_IDX     0x06
#define STAT_GPM_IDX        0x07
#define STAT_PPC_IDX        0x08 // Chlor level?
#define STAT_B09_IDX        0x09 // What's this?
#define STAT_ERR_IDX        0x10 // Whatever the error codes are. 0x00 is OK
#define STAT_B11_IDX        0x11 // What's this?
#define STAT_TIMER_IDX      0x12 // TIMER in minutes
#define STAT_CLK_HOUR_IDX   0x13
#define STAT_CLK_MIN_IDX    0x14
/* EOF - Pump message structure related stuff like indexes, constants, etc. */

//                             P R E A M B L E     VER   DST           SRC         CFI               LEN   DAT                    CHB   CLB (Check Sum is set when sending command, depends on addresses used)
byte setCtrlRemote[]    = {0xFF, 0x00, 0xFF, 0xA5, 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_CTRL,    0x01, IFLO_CTRL_REMOTE,      0x00, 0x00};
byte setCtrlLocal[]     = {0xFF, 0x00, 0xFF, 0xA5, 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_CTRL,    0x01, IFLO_CTRL_LOCAL,       0x00, 0x00};
byte getStatus[]        = {0xFF, 0x00, 0xFF, 0xA5, 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_STAT,    0x00,                        0x00, 0x00};
byte extProgOff[]       = {0xFF, 0x00, 0xFF, 0xA5, 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_REG,     0x04, (IFLO_REG_EPRG >> 8), (IFLO_REG_EPRG & 0xFF), (IFLO_EPRG_OFF >> 8), (IFLO_EPRG_OFF & 0xFF), 0x00, 0x00};
byte runExtProg1[]      = {0xFF, 0x00, 0xFF, 0xA5, 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_REG,     0x04, (IFLO_REG_EPRG >> 8), (IFLO_REG_EPRG & 0xFF), (IFLO_EPRG_P1 >> 8), (IFLO_EPRG_P1 & 0xFF), 0x00, 0x00};
byte runExtProg2[]      = {0xFF, 0x00, 0xFF, 0xA5, 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_REG,     0x04, (IFLO_REG_EPRG >> 8), (IFLO_REG_EPRG & 0xFF), (IFLO_EPRG_P2 >> 8), (IFLO_EPRG_P2 & 0xFF), 0x00, 0x00};
byte runExtProg3[]      = {0xFF, 0x00, 0xFF, 0xA5, 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_REG,     0x04, (IFLO_REG_EPRG >> 8), (IFLO_REG_EPRG & 0xFF), (IFLO_EPRG_P3 >> 8), (IFLO_EPRG_P3 & 0xFF), 0x00, 0x00};
byte runExtProg4[]      = {0xFF, 0x00, 0xFF, 0xA5, 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_REG,     0x04, (IFLO_REG_EPRG >> 8), (IFLO_REG_EPRG & 0xFF), (IFLO_EPRG_P4 >> 8), (IFLO_EPRG_P4 & 0xFF), 0x00, 0x00};
byte startPump[]        = {0xFF, 0x00, 0xFF, 0xA5, 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_RUN,     0x01, IFLO_RUN_STRT,         0x00, 0x00};
byte stopPump[]         = {0xFF, 0x00, 0xFF, 0xA5, 0x00, PUMP_ADDRESS, MY_ADDRESS, IFLO_CMD_RUN,     0x01, IFLO_RUN_STOP,         0x00, 0x00}; ///????

SoftwareSerial RS485Serial(SSerialRx, SSerialTx); // RX, TX

#define MAX_INPUT_LEN   128
uint8_t inputBuffer[MAX_INPUT_LEN];
uint8_t* bPointer = 0;
boolean inputComplete = false;

void setup() {
    Serial.begin(9600);
    Serial.println("RS485 Sender...");

    pinMode(SSerialTxControl, OUTPUT);

    pinMode(Pin13LED, OUTPUT);

    // Init Transceiver, we start with listening
    digitalWrite(SSerialTxControl, RS485Receive);

    // Start the software serial port, to another device
    RS485Serial.begin(9600);   // set the data rate

    memset(inputBuffer, 0, sizeof(inputBuffer));
    bPointer = inputBuffer; // Set the pointer to the beginning of the buffer
}

void loop() {
    digitalWrite(Pin13LED, LOW);   // turn LED on

    // Convert the input into a byte sequence. Each received byte is actually a character of a hex string
    // i.e. HEX '0A' is represented by the bytes 0x00 and 0x41. So we need to convert 0x00 and 0x41 to the hex string 0x0A
    if (Serial.available()) {
        int iRcv = -1;

        while ((iRcv = Serial.read()) > -1) {
            // Line ending indicates end of command
            if (iRcv == 0x0A || iRcv == 0x0D) {
                inputComplete = true;
            } else if (iRcv >= 0x30 && iRcv <= 0x39) {
               *bPointer++ = (byte)(iRcv - 0x30);
            } else if ((iRcv >= 0x41 && iRcv <= 0x46)) { // Upper case characters
                *bPointer++ = (byte)(iRcv - 0x37);
            } else if ((iRcv >= 0x61 && iRcv <= 0x66)) { // Lower case characters
                *bPointer++ = (byte)(iRcv - 0x57);
            } else {
                // noop, invalid HEX character. Reset input
            }

            // Reset the buffer
            if ((bPointer - inputBuffer) > MAX_INPUT_LEN) {
                resetInputBuffer();
            }
        }
    }

    // Now get the actual bytes from from the input
    if (inputComplete == true) {
        int noHexChars = (bPointer - inputBuffer); // Pointer arithmetic usually gives you the index, but we incremented the index after every assignment, so we get now the length

        if ((noHexChars % 2) == 0) {
            // Send input to RS485
            digitalWrite(SSerialTxControl, RS485Transmit);

            // Now read two 'hex characters' at a time from the input buffer and convert them into the actual byte value
            byte actualData[noHexChars / 2];
            for (int i = 0, j = 0; i < noHexChars; i += 2, ++j) {
                actualData[j] = inputBuffer[i + 1] + (inputBuffer[i] << 4);
            }

            Serial.print("Sending data: ");
            for (int i = 0; i < (noHexChars / 2); ++i) {
                byte b = actualData[i];
                Serial.print(b < 0x10 ? "0" + String(b, HEX) : String(b, HEX));

            }
            Serial.println();

            RS485Serial.write(actualData, (sizeof(actualData) / sizeof(byte)));

            // Put RS485 back into listening mode
            digitalWrite(SSerialTxControl, RS485Receive);
            digitalWrite(Pin13LED, HIGH);
        } else {
            Serial.println("Invalid input received");
        }

        // Reset the input buffer
        resetInputBuffer();
    }

    // Now the actual work... Look for data from RS485 and echo it to the serial port
    if (RS485Serial.available() > 0) {
        Serial.print("Receiving data: ");
        int iRcv = -1;
        while ((iRcv = RS485Serial.read()) > -1) {
            digitalWrite(Pin13LED, HIGH);
            byte b = (byte)iRcv;
            Serial.print(b < 0x10 ? "0" + String(b, HEX) : String(b, HEX));
            digitalWrite(Pin13LED, LOW);
        }
        Serial.println();

        digitalWrite(Pin13LED, HIGH);
    }
}

void resetInputBuffer() {
    inputComplete = false;
    memset(inputBuffer, 0, sizeof(inputBuffer));
    bPointer = inputBuffer;
}
