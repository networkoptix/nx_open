#ifndef __ENERROR_H__
#define __ENERROR_H__

#define BR_ERR_NO_ERROR                        0x00000000ul
#define BR_ERR_RESET_TIMEOUT                   0x00000001ul
#define BR_ERR_WRITE_REG_TIMEOUT               0x00000002ul
#define BR_ERR_WRITE_TUNER_TIMEOUT             0x00000003ul
#define BR_ERR_WRITE_TUNER_FAIL                0x00000004ul
#define BR_ERR_RSD_COUNTER_NOT_READY           0x00000005ul
#define BR_ERR_VTB_COUNTER_NOT_READY           0x00000006ul
#define BR_ERR_FEC_MON_NOT_ENABLED             0x00000007ul
#define BR_ERR_INVALID_DEV_TYPE                0x00000008ul
#define BR_ERR_INVALID_TUNER_TYPE              0x00000009ul
#define BR_ERR_OPEN_FILE_FAIL                  0x0000000Aul
#define BR_ERR_WRITEFILE_FAIL                  0x0000000Bul
#define BR_ERR_READFILE_FAIL                   0x0000000Cul
#define BR_ERR_CREATEFILE_FAIL                 0x0000000Dul
#define BR_ERR_MALLOC_FAIL                     0x0000000Eul
#define BR_ERR_INVALID_FILE_SIZE               0x0000000Ful
#define BR_ERR_INVALID_READ_SIZE               0x00000010ul
#define BR_ERR_LOAD_FW_DONE_BUT_FAIL           0x00000011ul
#define BR_ERR_NOT_IMPLEMENTED                 0x00000012ul
#define BR_ERR_NOT_SUPPORT                     0x00000013ul
#define BR_ERR_WRITE_MBX_TUNER_TIMEOUT         0x00000014ul
#define BR_ERR_DIV_MORE_THAN_8_CHIPS           0x00000015ul
#define BR_ERR_DIV_NO_CHIPS                    0x00000016ul
#define BR_ERR_SUPER_FRAME_CNT_0               0x00000017ul
#define BR_ERR_INVALID_FFT_MODE                0x00000018ul
#define BR_ERR_INVALID_CONSTELLATION_MODE      0x00000019ul
#define BR_ERR_RSD_PKT_CNT_0                   0x0000001Aul
#define BR_ERR_FFT_SHIFT_TIMEOUT               0x0000001Bul
#define BR_ERR_WAIT_TPS_TIMEOUT                0x0000001Cul
#define BR_ERR_INVALID_BW                      0x0000001Dul
#define BR_ERR_INVALID_BUF_LEN                 0x0000001Eul
#define BR_ERR_NULL_PTR                        0x0000001Ful
#define BR_ERR_INVALID_AGC_VOLT                0x00000020ul
#define BR_ERR_MT_OPEN_FAIL                    0x00000021ul
#define BR_ERR_MT_TUNE_FAIL                    0x00000022ul
#define BR_ERR_CMD_NOT_SUPPORTED               0x00000023ul
#define BR_ERR_CE_NOT_READY                    0x00000024ul
#define BR_ERR_EMBX_INT_NOT_CLEARED            0x00000025ul
#define BR_ERR_INV_PULLUP_VOLT                 0x00000026ul
#define BR_ERR_FREQ_OUT_OF_RANGE               0x00000027ul
#define BR_ERR_INDEX_OUT_OF_RANGE              0x00000028ul
#define BR_ERR_NULL_SETTUNER_PTR               0x00000029ul
#define BR_ERR_NULL_INITSCRIPT_PTR             0x0000002Aul
#define BR_ERR_INVALID_INITSCRIPT_LEN          0x0000002Bul
#define BR_ERR_INVALID_POS                     0x0000002Cul
#define BR_ERR_BACK_TO_BOOTCODE_FAIL           0x0000002Dul
#define BR_ERR_GET_BUFFER_VALUE_FAIL           0x0000002Eul
#define BR_ERR_INVALID_REG_VALUE               0x0000002Ful
#define BR_ERR_INVALID_INDEX                   0x00000030ul
#define BR_ERR_READ_TUNER_TIMEOUT              0x00000031ul
#define BR_ERR_READ_TUNER_FAIL                 0x00000032ul
#define BR_ERR_UNDEFINED_SAW_BW                0x00000033ul
#define BR_ERR_MT_NOT_AVAILABLE                0x00000034ul
#define BR_ERR_NO_SUCH_TABLE                   0x00000035ul
#define BR_ERR_WRONG_CHECKSUM                  0x00000036ul
#define BR_ERR_INVALID_XTAL_FREQ               0x00000037ul
#define BR_ERR_COUNTER_NOT_AVAILABLE           0x00000038ul
#define BR_ERR_INVALID_DATA_LENGTH             0x00000039ul
#define BR_ERR_BOOT_FAIL                       0x0000003Aul
#define BR_ERR_INITIALIZE_FAIL                 0x0000003Aul
#define BR_ERR_BUFFER_INSUFFICIENT             0x0000003Bul
#define BR_ERR_NOT_READY                       0x0000003Cul
#define BR_ERR_DRIVER_INVALID                  0x0000003Dul
#define BR_ERR_INTERFACE_FAIL                  0x0000003Eul
#define BR_ERR_PID_FILTER_FULL                 0x0000003Ful
#define BR_ERR_OPERATION_TIMEOUT               0x00000040ul
#define BR_ERR_LOADFIRMWARE_SKIPPED            0x00000041ul
#define BR_ERR_REBOOT_FAIL                     0x00000042ul
#define BR_ERR_PROTOCOL_FORMAT_INVALID         0x00000043ul
#define BR_ERR_ACTIVESYNC_ERROR                0x00000044ul
#define BR_ERR_CE_READWRITEBUS_ERROR           0x00000045ul
#define BR_ERR_CE_NODATA_ERROR                 0x00000046ul
#define BR_ERR_NULL_FW_SCRIPT                  0x00000047ul
#define BR_ERR_NULL_TUNER_SCRIPT               0x00000048ul
#define BR_ERR_INVALID_CHIP_TYPE               0x00000049ul
#define BR_ERR_TUNER_TYPE_NOT_COMPATIBLE       0x0000004Aul
#define BR_ERR_NULL_HANDLE_PTR                 0x0000004Bul
#define BR_ERR_BAUDRATE_NOT_SUPPORT			   0x0000004Cul

/** Error Code of System */
#define BR_ERR_INVALID_INDICATOR_TYPE          0x00000101ul
#define BR_ERR_INVALID_SC_NUMBER               0x00000102ul
#define BR_ERR_INVALID_SC_INFO                 0x00000103ul
#define BR_ERR_FIGBYPASS_FAIL                  0x00000104ul

/** Error Code of Firmware */
#define BR_ERR_FIRMWARE_STATUS                 0x01000000ul
#define BR_ERR_FW_I2CM_UPPER_CHIP_BUS_ERR      0x0100001Aul /** i2c bus number for 2nd, 3rd, and 4th endeavour is not set*/
#define BR_ERR_FW_NO_NEXT_LEVEL_CHIP           0x0100001Bul /** i2c address for omega is not set*/
#define BR_ERR_FW_NEXT_LEVEL_CHIP_BUS_ERR      0x0100001Cul /** i2c bus number for omega is not set */
#define BR_ERR_NEXT_LEVEL_CHIP_WRONG_CHECKSUM  0x0100001Eul /** endeavour gets check-sum error from omega */
#define BR_ERR_NEXT_LEVEL_CHIP_STATUS          0x0100001Ful /** endeavour gets error code from omega */

/** Error Code of I2C Module */
#define BR_ERR_I2C_DATA_HIGH_FAIL              0x02001000ul
#define BR_ERR_I2C_CLK_HIGH_FAIL               0x02002000ul
#define BR_ERR_I2C_WRITE_NO_ACK                0x02003000ul
#define BR_ERR_I2C_DATA_LOW_FAIL               0x02004000ul

/** Error Code of USB Module */
#define BR_ERR_USB_NULL_HANDLE                 0x03010001ul
#define BR_ERR_USB_WRITEFILE_FAIL              0x03000002ul
#define BR_ERR_USB_READFILE_FAIL               0x03000003ul
#define BR_ERR_USB_INVALID_READ_SIZE           0x03000004ul
#define BR_ERR_USB_INVALID_STATUS              0x03000005ul
#define BR_ERR_USB_INVALID_SN                  0x03000006ul
#define BR_ERR_USB_INVALID_PKT_SIZE            0x03000007ul
#define BR_ERR_USB_INVALID_HEADER              0x03000008ul
#define BR_ERR_USB_NO_IR_PKT                   0x03000009ul
#define BR_ERR_USB_INVALID_IR_PKT              0x0300000Aul
#define BR_ERR_USB_INVALID_DATA_LEN            0x0300000Bul
#define BR_ERR_USB_EP4_READFILE_FAIL           0x0300000Cul
#define BR_ERR_USB_EP$_INVALID_READ_SIZE       0x0300000Dul
#define BR_ERR_USB_BOOT_INVALID_PKT_TYPE       0x0300000Eul
#define BR_ERR_USB_BOOT_BAD_CONFIG_HEADER      0x0300000Ful
#define BR_ERR_USB_BOOT_BAD_CONFIG_SIZE        0x03000010ul
#define BR_ERR_USB_BOOT_BAD_CONFIG_SN          0x03000011ul
#define BR_ERR_USB_BOOT_BAD_CONFIG_SUBTYPE     0x03000012ul
#define BR_ERR_USB_BOOT_BAD_CONFIG_VALUE       0x03000013ul
#define BR_ERR_USB_BOOT_BAD_CONFIG_CHKSUM      0x03000014ul
#define BR_ERR_USB_BOOT_BAD_CONFIRM_HEADER     0x03000015ul
#define BR_ERR_USB_BOOT_BAD_CONFIRM_SIZE       0x03000016ul
#define BR_ERR_USB_BOOT_BAD_CONFIRM_SN         0x03000017ul
#define BR_ERR_USB_BOOT_BAD_CONFIRM_SUBTYPE    0x03000018ul
#define BR_ERR_USB_BOOT_BAD_CONFIRM_VALUE      0x03000019ul
#define BR_ERR_USB_BOOT_BAD_CONFIRM_CHKSUM     0x03000020ul
#define BR_ERR_USB_BOOT_BAD_BOOT_HEADER        0x03000021ul
#define BR_ERR_USB_BOOT_BAD_BOOT_SIZE          0x03000022ul
#define BR_ERR_USB_BOOT_BAD_BOOT_SN            0x03000023ul
#define BR_ERR_USB_BOOT_BAD_BOOT_PATTERN_01    0x03000024ul
#define BR_ERR_USB_BOOT_BAD_BOOT_PATTERN_10    0x03000025ul
#define BR_ERR_USB_BOOT_BAD_BOOT_CHKSUM        0x03000026ul
#define BR_ERR_USB_INVALID_BOOT_PKT_TYPE       0x03000027ul
#define BR_ERR_USB_BOOT_BAD_CONFIG_VAlUE       0x03000028ul
#define BR_ERR_USB_COINITIALIZEEX_FAIL         0x03000029ul
#define BR_ERR_USB_COCREATEINSTANCE_FAIL       0x0300003Aul
#define BR_ERR_USB_COCREATCLSEENUMERATOR_FAIL  0x0300002Bul
#define BR_ERR_USB_QUERY_INTERFACE_FAIL        0x0300002Cul
#define BR_ERR_USB_PKSCTRL_NULL                0x0300002Dul
#define BR_ERR_USB_INVALID_REGMODE             0x0300002Eul
#define BR_ERR_USB_INVALID_REG_COUNT           0x0300002Ful
#define BR_ERR_USB_INVALID_HANDLE              0x03000100ul
#define BR_ERR_USB_WRITE_FAIL                  0x03000200ul
#define BR_ERR_USB_UNEXPECTED_WRITE_LEN        0x03000300ul
#define BR_ERR_USB_READ_FAIL                   0x03000400ul

/** Error code of 9035U2I bridge*/
#define BR_ERR_AF9035U2I                       0x04000000ul
#define BR_ERR_EAGLESLAVE_FAIL                 0x04000000ul

/** Error code of Omega */                      
#define BR_ERR_TUNER_INIT_FAIL                 0x05000000ul
#define BR_ERR_OMEGA_TUNER_INIT_FAIL           0x05000000ul
#endif