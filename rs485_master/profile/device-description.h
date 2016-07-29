/*
 * Afero Device Profile header file
 * Device Description:		f39df645-b353-437c-a92d-bb106f24bfea
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
// Attribute Pump Command
#define AF_PUMP_COMMAND                                                 1
#define AF_PUMP_COMMAND_SZ                                              2
#define AF_PUMP_COMMAND_TYPE                        ATTRIBUTE_TYPE_SINT16

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

// Attribute Command
#define AF_SYSTEM_COMMAND                                           65012
#define AF_SYSTEM_COMMAND_SZ                                            4
#define AF_SYSTEM_COMMAND_TYPE                      ATTRIBUTE_TYPE_SINT32

// Attribute Hachi State
#define AF_SYSTEM_HACHI_STATE                                       65013
#define AF_SYSTEM_HACHI_STATE_SZ                                        1
#define AF_SYSTEM_HACHI_STATE_TYPE                   ATTRIBUTE_TYPE_SINT8

// Attribute Low Battery Warn
#define AF_SYSTEM_LOW_BATTERY_WARN                                  65014
#define AF_SYSTEM_LOW_BATTERY_WARN_SZ                                   1
#define AF_SYSTEM_LOW_BATTERY_WARN_TYPE              ATTRIBUTE_TYPE_SINT8

// Attribute Reboot Reason
#define AF_SYSTEM_REBOOT_REASON                                     65019
#define AF_SYSTEM_REBOOT_REASON_SZ                                    100
#define AF_SYSTEM_REBOOT_REASON_TYPE                 ATTRIBUTE_TYPE_UTF8S

// Attribute MCU Interface
#define AF_SYSTEM_MCU_INTERFACE                                     65021
#define AF_SYSTEM_MCU_INTERFACE_SZ                                      1
#define AF_SYSTEM_MCU_INTERFACE_TYPE               ATTRIBUTE_TYPE_BOOLEAN
//endregion
