//
// Created by Erhard Riedl on 6/9/16.
//

#ifndef RS485_MASTER_RS485_MASTER_H
#define RS485_MASTER_RS485_MASTER_H

#include "CommandQueue.h"

#define AFLIB_IN_USE
#define RUN_PROGRAM

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
#define	IFLO_MODE_SPEED1	0x02 /* Display speed 1 */
#define	IFLO_MODE_SPEED2	0x03 /* Display speed 2 */
#define	IFLO_MODE_SPEED3	0x04 /* Display speed 3 */
#define	IFLO_MODE_SPEED4	0x05 /* Display speed 4 */
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

//                                             P R E A M B L E                     VER   DST   SRC   CFI               LEN   DAT               CHB   CLB (Check Sum is set when sending command, depends on addresses used)
uint8_t cmdArrCtrlRemote[]  = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, 0x00, 0x00, IFLO_CMD_CTRL,    0x01, IFLO_CTRL_REMOTE, 0x00, 0x00};
uint8_t cmdArrCtrlLocal[]   = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, 0x00, 0x00, IFLO_CMD_CTRL,    0x01, IFLO_CTRL_LOCAL,  0x00, 0x00};
uint8_t cmdArrGetStatus[]   = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, 0x00, 0x00, IFLO_CMD_STAT,    0x00,                        0x00, 0x00};
uint8_t cmdArrExtProgOff[]  = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, 0x00, 0x00, IFLO_CMD_REG,     0x04, (IFLO_REG_EPRG >> 8), (IFLO_REG_EPRG & 0xFF), (IFLO_EPRG_OFF >> 8), (IFLO_EPRG_OFF & 0xFF), 0x00, 0x00};
uint8_t cmdArrRunExtProg1[] = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, 0x00, 0x00, IFLO_CMD_REG,     0x04, (IFLO_REG_EPRG >> 8), (IFLO_REG_EPRG & 0xFF), (IFLO_EPRG_P1 >> 8), (IFLO_EPRG_P1 & 0xFF), 0x00, 0x00};
uint8_t cmdArrRunExtProg2[] = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, 0x00, 0x00, IFLO_CMD_REG,     0x04, (IFLO_REG_EPRG >> 8), (IFLO_REG_EPRG & 0xFF), (IFLO_EPRG_P2 >> 8), (IFLO_EPRG_P2 & 0xFF), 0x00, 0x00};
uint8_t cmdArrRunExtProg3[] = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, 0x00, 0x00, IFLO_CMD_REG,     0x04, (IFLO_REG_EPRG >> 8), (IFLO_REG_EPRG & 0xFF), (IFLO_EPRG_P3 >> 8), (IFLO_EPRG_P3 & 0xFF), 0x00, 0x00};
uint8_t cmdArrRunExtProg4[] = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, 0x00, 0x00, IFLO_CMD_REG,     0x04, (IFLO_REG_EPRG >> 8), (IFLO_REG_EPRG & 0xFF), (IFLO_EPRG_P4 >> 8), (IFLO_EPRG_P4 & 0xFF), 0x00, 0x00};
uint8_t cmdArrStartPump[]   = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, 0x00, 0x00, IFLO_CMD_RUN,     0x01, IFLO_RUN_STRT,    0x00, 0x00};
uint8_t cmdArrStopPump[]    = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, 0x00, 0x00, IFLO_CMD_RUN,     0x01, IFLO_RUN_STOP,    0x00, 0x00};
uint8_t cmdArrSetSpeed1[]   = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, 0x00, 0x00, IFLO_CMD_MODE,    0x01, IFLO_MODE_SPEED1, 0x00, 0x00};
uint8_t cmdArrSetSpeed2[]   = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, 0x00, 0x00, IFLO_CMD_MODE,    0x01, IFLO_MODE_SPEED2, 0x00, 0x00};
uint8_t cmdArrSetSpeed3[]   = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, 0x00, 0x00, IFLO_CMD_MODE,    0x01, IFLO_MODE_SPEED3, 0x00, 0x00};
uint8_t cmdArrSetSpeed4[]   = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, 0x00, 0x00, IFLO_CMD_MODE,    0x01, IFLO_MODE_SPEED4, 0x00, 0x00};
uint8_t cmdArrCustom5[]     = {PREAMBLE[0], PREAMBLE[1], PREAMBLE[2], PREAMBLE[3], 0x00, 0x00, 0x00, IFLO_CMD_MODE,    0x01, 0x00, 0x00, 0x00};
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
#define MOSI                      51//11    // 51 on Mega 2560
#define MISO                      50//12    // 50 on Mega 2560
#define SCK                       52//13    // 52 on Mega 2560

#elif defined(TEENSYDUINO)
#define INT_PIN                   14    // Modulo uses this to initiate communication
#define CS_PIN                    10    // Standard SPI chip select (aka SS)
#define RESET                     21    // This is used to reboot the Modulo when the Teensy boots
#else
#error "Sorry, afLib does not support this board"
#endif
//endregion

//region RS485 constants
#define SSerialTx           7 // DI: data in
#define SSerialTxControl    6 // DE: data enable
//#define RE_PIN              10 // RE: receive enable; jumpered together with 6, will be put into floating mode
#define SSerialRx           62//11 // RO: receive out
#define RS485Transmit       HIGH
#define RS485Receive        LOW
//endregion

//region State machine
#define STATUS_QRY_TIMEOUT  1000 * 15   // 15 seconds
#define COMMAND_TIMEOUT     1000 * 2    // 2 second
#define EXT_PROG_RPT_INTVAL 1000 * 30   // 30 seconds

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
    CMD_NOOP,           //0
    CMD_RUN_PROG_1,     //1
    CMD_RUN_PROG_2,     //2
    CMD_RUN_PROG_3,     //3
    CMD_RUN_PROG_4,     //4
    CMD_SPEED_1,        //5
    CMD_SPEED_2,        //6
    CMD_SPEED_3,        //7
    CMD_SPEED_4,        //8
    CMD_STATUS,         //9
    CMD_EXT_PROG_OFF,   //10
    CMD_SCHEDULE_MODE,  //13
    CMD_PUMP_ON,        //14
    CMD_PUMP_OFF,       //15
    CMD_CTRL_REMOTE,    //16
    CMD_CTRL_LOCAL,     //17

    CMD_CUSTOM5,        //16
};

enum CONTROL_MODE {
    CTRL_MODE_REMOTE = IFLO_CTRL_REMOTE,
    CTRL_MODE_LOCAL = IFLO_CTRL_LOCAL
};

struct PumpStatus {
    uint8_t ctrl_mode;
    uint8_t pumpAddr;
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
#endif //RS485_MASTER_RS485_MASTER_H
