/*
 * Afero Device Profile header file
 * Device Description:		
 * Schema Version:	2
 */


#define ATTRIBUTE_TYPE_SINT8                                            2
#define ATTRIBUTE_TYPE_SINT16                                           3
#define ATTRIBUTE_TYPE_SINT32                                           4
#define ATTRIBUTE_TYPE_SINT64                                           5
#define ATTRIBUTE_TYPE_BOOLEAN                                          1
#define ATTRIBUTE_TYPE_UTF8S                                           20
#define ATTRIBUTE_TYPE_BYTES                                           21
#define ATTRIBUTE_TYPE_FLOAT32                                         10

//region Service ID 1
// Attribute Command
#define AF_COMMAND                                                      1
#define AF_COMMAND_SZ                                                   2
#define AF_COMMAND_TYPE                             ATTRIBUTE_TYPE_SINT16

// Attribute Set Controller Address
#define AF_SET_CONTROLLER_ADDRESS                                       2
#define AF_SET_CONTROLLER_ADDRESS_SZ                                    1
#define AF_SET_CONTROLLER_ADDRESS_TYPE               ATTRIBUTE_TYPE_SINT8

// Attribute Set Pump Address
#define AF_SET_PUMP_ADDRESS                                             3
#define AF_SET_PUMP_ADDRESS_SZ                                          1
#define AF_SET_PUMP_ADDRESS_TYPE                     ATTRIBUTE_TYPE_SINT8

// Attribute Status: Pump Running State
#define AF_STATUS__PUMP_RUNNING_STATE                                   4
#define AF_STATUS__PUMP_RUNNING_STATE_SZ                                1
#define AF_STATUS__PUMP_RUNNING_STATE_TYPE           ATTRIBUTE_TYPE_SINT8

// Attribute Status: Pump Mode
#define AF_STATUS__PUMP_MODE                                            5
#define AF_STATUS__PUMP_MODE_SZ                                         1
#define AF_STATUS__PUMP_MODE_TYPE                    ATTRIBUTE_TYPE_SINT8

// Attribute Status: Pump Drive State
#define AF_STATUS__PUMP_DRIVE_STATE                                     6
#define AF_STATUS__PUMP_DRIVE_STATE_SZ                                  1
#define AF_STATUS__PUMP_DRIVE_STATE_TYPE             ATTRIBUTE_TYPE_SINT8

// Attribute Status: Pump Power Usage (w)
#define AF_STATUS__PUMP_POWER_USAGE__W_                                 7
#define AF_STATUS__PUMP_POWER_USAGE__W__SZ                              2
#define AF_STATUS__PUMP_POWER_USAGE__W__TYPE        ATTRIBUTE_TYPE_SINT16

// Attribute Status: Pump Speed (rpm)
#define AF_STATUS__PUMP_SPEED__RPM_                                     8
#define AF_STATUS__PUMP_SPEED__RPM__SZ                                  2
#define AF_STATUS__PUMP_SPEED__RPM__TYPE            ATTRIBUTE_TYPE_SINT16

// Attribute Status: Pump Flow Rate (gpm)
#define AF_STATUS__PUMP_FLOW_RATE__GPM_                                 9
#define AF_STATUS__PUMP_FLOW_RATE__GPM__SZ                              1
#define AF_STATUS__PUMP_FLOW_RATE__GPM__TYPE         ATTRIBUTE_TYPE_SINT8

// Attribute Status: Pump PPC Levels (%)
#define AF_STATUS__PUMP_PPC_LEVELS____                                 10
#define AF_STATUS__PUMP_PPC_LEVELS_____SZ                               1
#define AF_STATUS__PUMP_PPC_LEVELS_____TYPE          ATTRIBUTE_TYPE_SINT8

// Attribute Status: Pump Byte 09 (?)
#define AF_STATUS__PUMP_BYTE_09____                                    11
#define AF_STATUS__PUMP_BYTE_09_____SZ                                  1
#define AF_STATUS__PUMP_BYTE_09_____TYPE             ATTRIBUTE_TYPE_SINT8

// Attribute Status: Pump Error Code
#define AF_STATUS__PUMP_ERROR_CODE                                     12
#define AF_STATUS__PUMP_ERROR_CODE_SZ                                   1
#define AF_STATUS__PUMP_ERROR_CODE_TYPE              ATTRIBUTE_TYPE_SINT8

// Attribute Status: Pump Byte 11 (?)
#define AF_STATUS__PUMP_BYTE_11____                                    13
#define AF_STATUS__PUMP_BYTE_11_____SZ                                  1
#define AF_STATUS__PUMP_BYTE_11_____TYPE             ATTRIBUTE_TYPE_SINT8

// Attribute Status: Pump Timer (min)
#define AF_STATUS__PUMP_TIMER__MIN_                                    14
#define AF_STATUS__PUMP_TIMER__MIN__SZ                                  2
#define AF_STATUS__PUMP_TIMER__MIN__TYPE            ATTRIBUTE_TYPE_SINT16

// Attribute Status: Pump Clock (HH:mm)
#define AF_STATUS__PUMP_CLOCK__HH_MM_                                  15
#define AF_STATUS__PUMP_CLOCK__HH_MM__SZ                                5
#define AF_STATUS__PUMP_CLOCK__HH_MM__TYPE           ATTRIBUTE_TYPE_UTF8S

// Attribute Status: Current Pump Address
#define AF_STATUS__CURRENT_PUMP_ADDRESS                                16
#define AF_STATUS__CURRENT_PUMP_ADDRESS_SZ                              1
#define AF_STATUS__CURRENT_PUMP_ADDRESS_TYPE         ATTRIBUTE_TYPE_SINT8

// Attribute Bootloader Version
#define AF_BOOTLOADER_VERSION                                        2001
#define AF_BOOTLOADER_VERSION_SZ                                        8
#define AF_BOOTLOADER_VERSION_TYPE                  ATTRIBUTE_TYPE_SINT64

// Attribute Softdevice Version
#define AF_SOFTDEVICE_VERSION                                        2002
#define AF_SOFTDEVICE_VERSION_SZ                                        8
#define AF_SOFTDEVICE_VERSION_TYPE                  ATTRIBUTE_TYPE_SINT64

// Attribute Application Version
#define AF_APPLICATION_VERSION                                       2003
#define AF_APPLICATION_VERSION_SZ                                       8
#define AF_APPLICATION_VERSION_TYPE                 ATTRIBUTE_TYPE_SINT64

// Attribute Profile Version
#define AF_PROFILE_VERSION                                           2004
#define AF_PROFILE_VERSION_SZ                                           8
#define AF_PROFILE_VERSION_TYPE                     ATTRIBUTE_TYPE_SINT64

// Attribute Security Enabled
#define AF_SECURITY_ENABLED                                         60000
#define AF_SECURITY_ENABLED_SZ                                          1
#define AF_SECURITY_ENABLED_TYPE                   ATTRIBUTE_TYPE_BOOLEAN

// Attribute Command
#define AF_COMMAND                                                  65012
#define AF_COMMAND_SZ                                                   4
#define AF_COMMAND_TYPE                             ATTRIBUTE_TYPE_SINT32

// Attribute Hachi State
#define AF_HACHI_STATE                                              65013
#define AF_HACHI_STATE_SZ                                               1
#define AF_HACHI_STATE_TYPE                          ATTRIBUTE_TYPE_SINT8

// Attribute Low Battery Warn
#define AF_LOW_BATTERY_WARN                                         65014
#define AF_LOW_BATTERY_WARN_SZ                                          1
#define AF_LOW_BATTERY_WARN_TYPE                     ATTRIBUTE_TYPE_SINT8

// Attribute Linked Timestamp
#define AF_LINKED_TIMESTAMP                                         65015
#define AF_LINKED_TIMESTAMP_SZ                                          4
#define AF_LINKED_TIMESTAMP_TYPE                    ATTRIBUTE_TYPE_SINT32

// Attribute Advertising Secret Duration
#define AF_ADVERTISING_SECRET_DURATION                              65016
#define AF_ADVERTISING_SECRET_DURATION_SZ                               1
#define AF_ADVERTISING_SECRET_DURATION_TYPE          ATTRIBUTE_TYPE_SINT8

// Attribute Advertising Secret
#define AF_ADVERTISING_SECRET                                       65017
#define AF_ADVERTISING_SECRET_SZ                                       32
#define AF_ADVERTISING_SECRET_TYPE                   ATTRIBUTE_TYPE_BYTES

// Attribute Attribute ACK
#define AF_ATTRIBUTE_ACK                                            65018
#define AF_ATTRIBUTE_ACK_SZ                                             2
#define AF_ATTRIBUTE_ACK_TYPE                       ATTRIBUTE_TYPE_SINT16

// Attribute Reboot Reason
#define AF_REBOOT_REASON                                            65019
#define AF_REBOOT_REASON_SZ                                           100
#define AF_REBOOT_REASON_TYPE                        ATTRIBUTE_TYPE_UTF8S

// Attribute BLE Comms
#define AF_BLE_COMMS                                                65020
#define AF_BLE_COMMS_SZ                                                12
#define AF_BLE_COMMS_TYPE                            ATTRIBUTE_TYPE_BYTES

// Attribute SPI Enabled
#define AF_SPI_ENABLED                                              65021
#define AF_SPI_ENABLED_SZ                                               1
#define AF_SPI_ENABLED_TYPE                        ATTRIBUTE_TYPE_BOOLEAN
//endregion
