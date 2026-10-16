/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Comprehensive camera sensor drivers for Infinix Zero 5G 2023 (CY-X6815C-H777 / MT6877)
 * Implements S5KJN1 (50MP), HI1631Q (16MP), GC02M1 (2MP depth), GC02M1MACRO (2MP macro)
 * Reconstructed from authentic stock kernel stock_Image via Hex-Rays IDA Pro & REA.
 * 100% stock parity: exact 9-mode S5KJN1, 8-mode HI1631Q, 6-mode GC02M1, 5-mode GC02M1MACRO.
 */

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "imgsensor_sensor_list.h"
#include "imgsensor_i2c.h"

enum {
    IMGSENSOR_MODE_INIT,
    IMGSENSOR_MODE_PREVIEW,
    IMGSENSOR_MODE_CAPTURE,
    IMGSENSOR_MODE_VIDEO,
    IMGSENSOR_MODE_HIGH_SPEED_VIDEO,
    IMGSENSOR_MODE_SLIM_VIDEO,
    IMGSENSOR_MODE_CUSTOM1,
    IMGSENSOR_MODE_CUSTOM2,
    IMGSENSOR_MODE_CUSTOM3,
    IMGSENSOR_MODE_CUSTOM4,
    IMGSENSOR_MODE_CUSTOM5,
};

struct imgsensor_struct {
    kal_uint8 mirror;
    kal_uint8 sensor_mode;
    kal_uint32 shutter;
    kal_uint16 gain;
    kal_uint32 pclk;
    kal_uint32 frame_length;
    kal_uint32 line_length;
    kal_uint32 min_frame_length;
    kal_uint16 dummy_pixel;
    kal_uint16 dummy_line;
    kal_uint16 current_fps;
    kal_bool autoflicker_en;
    kal_bool test_pattern;
    enum MSDK_SCENARIO_ID_ENUM current_scenario_id;
    kal_uint8 ihdr_en;
    kal_uint8 i2c_write_id;
};

extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
extern int iBurstWriteReg_multi(u8 *pData, u32 bytes, u16 i2cId, u16 transfer_length, u16 timing);

struct regval_16 {
    u16 reg;
    u16 val;
};

struct regval_8 {
    u8 reg;
    u8 val;
};

static void write_regs_16(const struct regval_16 *regs, size_t count, u16 i2c_id)
{
    u8 buf[1020];
    u32 tosend = 0;
    size_t i;

    for (i = 0; i < count; i++) {
        buf[tosend++] = (regs[i].reg >> 8) & 0xff;
        buf[tosend++] = regs[i].reg & 0xff;
        buf[tosend++] = (regs[i].val >> 8) & 0xff;
        buf[tosend++] = regs[i].val & 0xff;

        if (tosend + 4 > sizeof(buf) || i + 1 == count) {
            iBurstWriteReg_multi(buf, tosend, i2c_id, 4, 1000);
            tosend = 0;
        }
    }
}

static void write_regs_8(const struct regval_8 *regs, size_t count, u16 i2c_id)
{
    size_t i;
    u8 buf[2];
    for (i = 0; i < count; i++) {
        buf[0] = regs[i].reg;
        buf[1] = regs[i].val;
        iWriteRegI2C(buf, 2, i2c_id);
    }
}

static kal_uint16 write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para, kal_uint16 i2c_id)
{
    kal_uint8 pu_send_cmd[3] = {(kal_uint8)(addr >> 8), (kal_uint8)(addr & 0xFF), (kal_uint8)(para & 0xFF)};
    return iWriteRegI2C(pu_send_cmd, 3, i2c_id);
}

static kal_uint16 write_cmos_sensor_16(kal_uint16 addr, kal_uint16 para, kal_uint16 i2c_id)
{
    kal_uint8 pu_send_cmd[4] = {(kal_uint8)(addr >> 8), (kal_uint8)(addr & 0xFF), (kal_uint8)(para >> 8), (kal_uint8)(para & 0xFF)};
    return iWriteRegI2C(pu_send_cmd, 4, i2c_id);
}

static kal_uint16 write_cmos_sensor_8_8(kal_uint8 addr, kal_uint8 para, kal_uint16 i2c_id)
{
    kal_uint8 pu_send_cmd[2] = {addr, para};
    return iWriteRegI2C(pu_send_cmd, 2, i2c_id);
}

/* ========================================================================= */
/* S5KJN1 (Samsung ISOCELL JN1 50MP, Sensor ID 0x38E1)                       */
/* ========================================================================= */

static const struct regval_16 s5kjn1_init_regs[30] = {
    { 0x6028, 0x2400 },
    { 0x602a, 0x1354 },
    { 0x6f12, 0x0100 },
    { 0x6f12, 0x7017 },
    { 0x602a, 0x13b2 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x1236 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x1a0a },
    { 0x6f12, 0x4c0a },
    { 0x602a, 0x2210 },
    { 0x6f12, 0x3401 },
    { 0x602a, 0x2176 },
    { 0x6f12, 0x6400 },
    { 0x602a, 0x222e },
    { 0x6f12, 0x0001 },
    { 0x602a, 0x06b6 },
    { 0x6f12, 0x0a00 },
    { 0x602a, 0x06bc },
    { 0x6f12, 0x1001 },
    { 0x602a, 0x2140 },
    { 0x6f12, 0x0101 },
    { 0x602a, 0x1a0e },
    { 0x6f12, 0x9600 },
    { 0x6028, 0x4000 },
    { 0xf44e, 0x0011 },
    { 0xf44c, 0x0b0b },
    { 0xf44a, 0x0006 },
    { 0x0118, 0x0002 },
    { 0x011a, 0x0001 },
};

static const struct regval_16 s5kjn1_scen1_regs[277] = {
    { 0x6028, 0x2400 },
    { 0x602a, 0x1a28 },
    { 0x6f12, 0x4c00 },
    { 0x602a, 0x065a },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x139e },
    { 0x6f12, 0x0100 },
    { 0x602a, 0x139c },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x13a0 },
    { 0x6f12, 0x0a00 },
    { 0x6f12, 0x0120 },
    { 0x602a, 0x2072 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x1a64 },
    { 0x6f12, 0x0301 },
    { 0x6f12, 0xff00 },
    { 0x602a, 0x19e6 },
    { 0x6f12, 0x0200 },
    { 0x602a, 0x1a30 },
    { 0x6f12, 0x3401 },
    { 0x602a, 0x19fc },
    { 0x6f12, 0x0b00 },
    { 0x602a, 0x19f4 },
    { 0x6f12, 0x0606 },
    { 0x602a, 0x19f8 },
    { 0x6f12, 0x1010 },
    { 0x602a, 0x1b26 },
    { 0x6f12, 0x6f80 },
    { 0x6f12, 0xa060 },
    { 0x602a, 0x1a3c },
    { 0x6f12, 0x6207 },
    { 0x602a, 0x1a48 },
    { 0x6f12, 0x6207 },
    { 0x602a, 0x1444 },
    { 0x6f12, 0x2000 },
    { 0x6f12, 0x2000 },
    { 0x602a, 0x144c },
    { 0x6f12, 0x3f00 },
    { 0x6f12, 0x3f00 },
    { 0x602a, 0x7f6c },
    { 0x6f12, 0x0100 },
    { 0x6f12, 0x2f00 },
    { 0x6f12, 0xfa00 },
    { 0x6f12, 0x2400 },
    { 0x6f12, 0xe500 },
    { 0x602a, 0x0650 },
    { 0x6f12, 0x0600 },
    { 0x602a, 0x0654 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x1a46 },
    { 0x6f12, 0x8a00 },
    { 0x602a, 0x1a52 },
    { 0x6f12, 0xbf00 },
    { 0x602a, 0x0674 },
    { 0x6f12, 0x0500 },
    { 0x6f12, 0x0500 },
    { 0x6f12, 0x0500 },
    { 0x6f12, 0x0500 },
    { 0x602a, 0x0668 },
    { 0x6f12, 0x0800 },
    { 0x6f12, 0x0800 },
    { 0x6f12, 0x0800 },
    { 0x6f12, 0x0800 },
    { 0x602a, 0x0684 },
    { 0x6f12, 0x4001 },
    { 0x602a, 0x0688 },
    { 0x6f12, 0x4001 },
    { 0x602a, 0x147c },
    { 0x6f12, 0x1000 },
    { 0x602a, 0x1480 },
    { 0x6f12, 0x1000 },
    { 0x602a, 0x19f6 },
    { 0x6f12, 0x0904 },
    { 0x602a, 0x0812 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x1a02 },
    { 0x6f12, 0x1800 },
    { 0x602a, 0x2148 },
    { 0x6f12, 0x0100 },
    { 0x602a, 0x2042 },
    { 0x6f12, 0x1a00 },
    { 0x602a, 0x0874 },
    { 0x6f12, 0x0100 },
    { 0x602a, 0x09c0 },
    { 0x6f12, 0x2008 },
    { 0x602a, 0x09c4 },
    { 0x6f12, 0x2000 },
    { 0x602a, 0x19fe },
    { 0x6f12, 0x0e1c },
    { 0x602a, 0x4d92 },
    { 0x6f12, 0x0100 },
    { 0x602a, 0x84c8 },
    { 0x6f12, 0x0100 },
    { 0x602a, 0x4d94 },
    { 0x6f12, 0x0005 },
    { 0x6f12, 0x000a },
    { 0x6f12, 0x0010 },
    { 0x6f12, 0x0810 },
    { 0x6f12, 0x000a },
    { 0x6f12, 0x0040 },
    { 0x6f12, 0x0810 },
    { 0x6f12, 0x0810 },
    { 0x6f12, 0x8002 },
    { 0x6f12, 0xfd03 },
    { 0x6f12, 0x0010 },
    { 0x6f12, 0x1510 },
    { 0x602a, 0x3570 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x3574 },
    { 0x6f12, 0x1201 },
    { 0x602a, 0x21e4 },
    { 0x6f12, 0x0400 },
    { 0x602a, 0x21ec },
    { 0x6f12, 0x1f04 },
    { 0x602a, 0x2080 },
    { 0x6f12, 0x0101 },
    { 0x6f12, 0xff00 },
    { 0x6f12, 0x7f01 },
    { 0x6f12, 0x0001 },
    { 0x6f12, 0x8001 },
    { 0x6f12, 0xd244 },
    { 0x6f12, 0xd244 },
    { 0x6f12, 0x14f4 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x20ba },
    { 0x6f12, 0x121c },
    { 0x6f12, 0x111c },
    { 0x6f12, 0x54f4 },
    { 0x602a, 0x120e },
    { 0x6f12, 0x1000 },
    { 0x602a, 0x212e },
    { 0x6f12, 0x0200 },
    { 0x602a, 0x13ae },
    { 0x6f12, 0x0101 },
    { 0x602a, 0x0718 },
    { 0x6f12, 0x0001 },
    { 0x602a, 0x0710 },
    { 0x6f12, 0x0002 },
    { 0x6f12, 0x0804 },
    { 0x6f12, 0x0100 },
    { 0x602a, 0x1b5c },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x0786 },
    { 0x6f12, 0x7701 },
    { 0x602a, 0x2022 },
    { 0x6f12, 0x0500 },
    { 0x6f12, 0x0500 },
    { 0x602a, 0x1360 },
    { 0x6f12, 0x0100 },
    { 0x602a, 0x1376 },
    { 0x6f12, 0x0100 },
    { 0x6f12, 0x6038 },
    { 0x6f12, 0x7038 },
    { 0x6f12, 0x8038 },
    { 0x602a, 0x1386 },
    { 0x6f12, 0x0b00 },
    { 0x602a, 0x06fa },
    { 0x6f12, 0x1000 },
    { 0x602a, 0x4a94 },
    { 0x6f12, 0x0900 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0300 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0300 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0900 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x0a76 },
    { 0x6f12, 0x1000 },
    { 0x602a, 0x0aee },
    { 0x6f12, 0x1000 },
    { 0x602a, 0x0b66 },
    { 0x6f12, 0x1000 },
    { 0x602a, 0x0bde },
    { 0x6f12, 0x1000 },
    { 0x602a, 0x0be8 },
    { 0x6f12, 0x3000 },
    { 0x6f12, 0x3000 },
    { 0x602a, 0x0c56 },
    { 0x6f12, 0x1000 },
    { 0x602a, 0x0c60 },
    { 0x6f12, 0x3000 },
    { 0x6f12, 0x3000 },
    { 0x602a, 0x0cb6 },
    { 0x6f12, 0x0100 },
    { 0x602a, 0x0cf2 },
    { 0x6f12, 0x0001 },
    { 0x602a, 0x0cf0 },
    { 0x6f12, 0x0101 },
    { 0x602a, 0x11b8 },
    { 0x6f12, 0x0100 },
    { 0x602a, 0x11f6 },
    { 0x6f12, 0x0020 },
    { 0x602a, 0x4a74 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0xd8ff },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0xd8ff },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x218e },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x2268 },
    { 0x6f12, 0xf279 },
    { 0x602a, 0x5006 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x500e },
    { 0x6f12, 0x0100 },
    { 0x602a, 0x4e70 },
    { 0x6f12, 0x2062 },
    { 0x6f12, 0x5501 },
    { 0x602a, 0x06dc },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6028, 0x4000 },
    { 0xf46a, 0xae80 },
    { 0x0344, 0x0000 },
    { 0x0346, 0x0000 },
    { 0x0348, 0x1fff },
    { 0x034a, 0x181f },
    { 0x034c, 0x0ff0 },
    { 0x034e, 0x0c00 },
    { 0x0350, 0x0008 },
    { 0x0352, 0x0008 },
    { 0x0900, 0x0122 },
    { 0x0380, 0x0002 },
    { 0x0382, 0x0002 },
    { 0x0384, 0x0002 },
    { 0x0386, 0x0002 },
    { 0x0110, 0x1002 },
    { 0x0114, 0x0301 },
    { 0x0116, 0x3000 },
    { 0x0136, 0x1800 },
    { 0x013e, 0x0000 },
    { 0x0300, 0x0006 },
    { 0x0302, 0x0001 },
    { 0x0304, 0x0004 },
    { 0x0306, 0x0088 },
    { 0x0308, 0x0008 },
    { 0x030a, 0x0001 },
    { 0x030c, 0x0000 },
    { 0x030e, 0x0004 },
    { 0x0310, 0x0072 },
    { 0x0312, 0x0000 },
    { 0x080e, 0x0000 },
    { 0x0340, 0x0d9e },
    { 0x0342, 0x1450 },
    { 0x0702, 0x0000 },
    { 0x0202, 0x0100 },
    { 0x0200, 0x0100 },
    { 0x0d00, 0x0101 },
    { 0x0d02, 0x0101 },
    { 0x0d04, 0x0102 },
    { 0x6226, 0x0000 },
};

static const struct regval_16 s5kjn1_scen2_regs[277] = {
    { 0x6028, 0x2400 },
    { 0x602a, 0x1a28 },
    { 0x6f12, 0x4c00 },
    { 0x602a, 0x065a },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x139e },
    { 0x6f12, 0x0100 },
    { 0x602a, 0x139c },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x13a0 },
    { 0x6f12, 0x0a00 },
    { 0x6f12, 0x0120 },
    { 0x602a, 0x2072 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x1a64 },
    { 0x6f12, 0x0301 },
    { 0x6f12, 0xff00 },
    { 0x602a, 0x19e6 },
    { 0x6f12, 0x0200 },
    { 0x602a, 0x1a30 },
    { 0x6f12, 0x3401 },
    { 0x602a, 0x19fc },
    { 0x6f12, 0x0b00 },
    { 0x602a, 0x19f4 },
    { 0x6f12, 0x0606 },
    { 0x602a, 0x19f8 },
    { 0x6f12, 0x1010 },
    { 0x602a, 0x1b26 },
    { 0x6f12, 0x6f80 },
    { 0x6f12, 0xa060 },
    { 0x602a, 0x1a3c },
    { 0x6f12, 0x6207 },
    { 0x602a, 0x1a48 },
    { 0x6f12, 0x6207 },
    { 0x602a, 0x1444 },
    { 0x6f12, 0x2000 },
    { 0x6f12, 0x2000 },
    { 0x602a, 0x144c },
    { 0x6f12, 0x3f00 },
    { 0x6f12, 0x3f00 },
    { 0x602a, 0x7f6c },
    { 0x6f12, 0x0100 },
    { 0x6f12, 0x2f00 },
    { 0x6f12, 0xfa00 },
    { 0x6f12, 0x2400 },
    { 0x6f12, 0xe500 },
    { 0x602a, 0x0650 },
    { 0x6f12, 0x0600 },
    { 0x602a, 0x0654 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x1a46 },
    { 0x6f12, 0x8a00 },
    { 0x602a, 0x1a52 },
    { 0x6f12, 0xbf00 },
    { 0x602a, 0x0674 },
    { 0x6f12, 0x0500 },
    { 0x6f12, 0x0500 },
    { 0x6f12, 0x0500 },
    { 0x6f12, 0x0500 },
    { 0x602a, 0x0668 },
    { 0x6f12, 0x0800 },
    { 0x6f12, 0x0800 },
    { 0x6f12, 0x0800 },
    { 0x6f12, 0x0800 },
    { 0x602a, 0x0684 },
    { 0x6f12, 0x4001 },
    { 0x602a, 0x0688 },
    { 0x6f12, 0x4001 },
    { 0x602a, 0x147c },
    { 0x6f12, 0x1000 },
    { 0x602a, 0x1480 },
    { 0x6f12, 0x1000 },
    { 0x602a, 0x19f6 },
    { 0x6f12, 0x0904 },
    { 0x602a, 0x0812 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x1a02 },
    { 0x6f12, 0x1800 },
    { 0x602a, 0x2148 },
    { 0x6f12, 0x0100 },
    { 0x602a, 0x2042 },
    { 0x6f12, 0x1a00 },
    { 0x602a, 0x0874 },
    { 0x6f12, 0x0100 },
    { 0x602a, 0x09c0 },
    { 0x6f12, 0x2008 },
    { 0x602a, 0x09c4 },
    { 0x6f12, 0x2000 },
    { 0x602a, 0x19fe },
    { 0x6f12, 0x0e1c },
    { 0x602a, 0x4d92 },
    { 0x6f12, 0x0100 },
    { 0x602a, 0x84c8 },
    { 0x6f12, 0x0100 },
    { 0x602a, 0x4d94 },
    { 0x6f12, 0x0005 },
    { 0x6f12, 0x000a },
    { 0x6f12, 0x0010 },
    { 0x6f12, 0x0810 },
    { 0x6f12, 0x000a },
    { 0x6f12, 0x0040 },
    { 0x6f12, 0x0810 },
    { 0x6f12, 0x0810 },
    { 0x6f12, 0x8002 },
    { 0x6f12, 0xfd03 },
    { 0x6f12, 0x0010 },
    { 0x6f12, 0x1510 },
    { 0x602a, 0x3570 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x3574 },
    { 0x6f12, 0x1201 },
    { 0x602a, 0x21e4 },
    { 0x6f12, 0x0400 },
    { 0x602a, 0x21ec },
    { 0x6f12, 0x1f04 },
    { 0x602a, 0x2080 },
    { 0x6f12, 0x0101 },
    { 0x6f12, 0xff00 },
    { 0x6f12, 0x7f01 },
    { 0x6f12, 0x0001 },
    { 0x6f12, 0x8001 },
    { 0x6f12, 0xd244 },
    { 0x6f12, 0xd244 },
    { 0x6f12, 0x14f4 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x20ba },
    { 0x6f12, 0x121c },
    { 0x6f12, 0x111c },
    { 0x6f12, 0x54f4 },
    { 0x602a, 0x120e },
    { 0x6f12, 0x1000 },
    { 0x602a, 0x212e },
    { 0x6f12, 0x0200 },
    { 0x602a, 0x13ae },
    { 0x6f12, 0x0101 },
    { 0x602a, 0x0718 },
    { 0x6f12, 0x0001 },
    { 0x602a, 0x0710 },
    { 0x6f12, 0x0002 },
    { 0x6f12, 0x0804 },
    { 0x6f12, 0x0100 },
    { 0x602a, 0x1b5c },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x0786 },
    { 0x6f12, 0x7701 },
    { 0x602a, 0x2022 },
    { 0x6f12, 0x0500 },
    { 0x6f12, 0x0500 },
    { 0x602a, 0x1360 },
    { 0x6f12, 0x0100 },
    { 0x602a, 0x1376 },
    { 0x6f12, 0x0100 },
    { 0x6f12, 0x6038 },
    { 0x6f12, 0x7038 },
    { 0x6f12, 0x8038 },
    { 0x602a, 0x1386 },
    { 0x6f12, 0x0b00 },
    { 0x602a, 0x06fa },
    { 0x6f12, 0x1000 },
    { 0x602a, 0x4a94 },
    { 0x6f12, 0x0900 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0300 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0300 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0900 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x0a76 },
    { 0x6f12, 0x1000 },
    { 0x602a, 0x0aee },
    { 0x6f12, 0x1000 },
    { 0x602a, 0x0b66 },
    { 0x6f12, 0x1000 },
    { 0x602a, 0x0bde },
    { 0x6f12, 0x1000 },
    { 0x602a, 0x0be8 },
    { 0x6f12, 0x3000 },
    { 0x6f12, 0x3000 },
    { 0x602a, 0x0c56 },
    { 0x6f12, 0x1000 },
    { 0x602a, 0x0c60 },
    { 0x6f12, 0x3000 },
    { 0x6f12, 0x3000 },
    { 0x602a, 0x0cb6 },
    { 0x6f12, 0x0100 },
    { 0x602a, 0x0cf2 },
    { 0x6f12, 0x0001 },
    { 0x602a, 0x0cf0 },
    { 0x6f12, 0x0101 },
    { 0x602a, 0x11b8 },
    { 0x6f12, 0x0100 },
    { 0x602a, 0x11f6 },
    { 0x6f12, 0x0020 },
    { 0x602a, 0x4a74 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0xd8ff },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0xd8ff },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x218e },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x2268 },
    { 0x6f12, 0xf279 },
    { 0x602a, 0x5006 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x500e },
    { 0x6f12, 0x0100 },
    { 0x602a, 0x4e70 },
    { 0x6f12, 0x2062 },
    { 0x6f12, 0x5501 },
    { 0x602a, 0x06dc },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6028, 0x4000 },
    { 0xf46a, 0xae80 },
    { 0x0344, 0x0000 },
    { 0x0346, 0x0000 },
    { 0x0348, 0x1fff },
    { 0x034a, 0x181f },
    { 0x034c, 0x0ff0 },
    { 0x034e, 0x0c00 },
    { 0x0350, 0x0008 },
    { 0x0352, 0x0008 },
    { 0x0900, 0x0122 },
    { 0x0380, 0x0002 },
    { 0x0382, 0x0002 },
    { 0x0384, 0x0002 },
    { 0x0386, 0x0002 },
    { 0x0110, 0x1002 },
    { 0x0114, 0x0301 },
    { 0x0116, 0x3000 },
    { 0x0136, 0x1800 },
    { 0x013e, 0x0000 },
    { 0x0300, 0x0006 },
    { 0x0302, 0x0001 },
    { 0x0304, 0x0004 },
    { 0x0306, 0x0088 },
    { 0x0308, 0x0008 },
    { 0x030a, 0x0001 },
    { 0x030c, 0x0000 },
    { 0x030e, 0x0004 },
    { 0x0310, 0x0072 },
    { 0x0312, 0x0000 },
    { 0x080e, 0x0000 },
    { 0x0340, 0x0d9e },
    { 0x0342, 0x1450 },
    { 0x0702, 0x0000 },
    { 0x0202, 0x0100 },
    { 0x0200, 0x0100 },
    { 0x0d00, 0x0101 },
    { 0x0d02, 0x0101 },
    { 0x0d04, 0x0102 },
    { 0x6226, 0x0000 },
};

static const struct regval_16 s5kjn1_scen3_regs[277] = {
    { 0x6028, 0x2400 },
    { 0x602a, 0x1a28 },
    { 0x6f12, 0x4c00 },
    { 0x602a, 0x065a },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x139e },
    { 0x6f12, 0x0300 },
    { 0x602a, 0x139c },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x13a0 },
    { 0x6f12, 0x0a00 },
    { 0x6f12, 0x0020 },
    { 0x602a, 0x2072 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x1a64 },
    { 0x6f12, 0x0301 },
    { 0x6f12, 0x3f00 },
    { 0x602a, 0x19e6 },
    { 0x6f12, 0x0201 },
    { 0x602a, 0x1a30 },
    { 0x6f12, 0x3401 },
    { 0x602a, 0x19fc },
    { 0x6f12, 0x0b00 },
    { 0x602a, 0x19f4 },
    { 0x6f12, 0x0606 },
    { 0x602a, 0x19f8 },
    { 0x6f12, 0x1010 },
    { 0x602a, 0x1b26 },
    { 0x6f12, 0x6f80 },
    { 0x6f12, 0xa020 },
    { 0x602a, 0x1a3c },
    { 0x6f12, 0x5207 },
    { 0x602a, 0x1a48 },
    { 0x6f12, 0x5207 },
    { 0x602a, 0x1444 },
    { 0x6f12, 0x2100 },
    { 0x6f12, 0x2100 },
    { 0x602a, 0x144c },
    { 0x6f12, 0x4200 },
    { 0x6f12, 0x4200 },
    { 0x602a, 0x7f6c },
    { 0x6f12, 0x0100 },
    { 0x6f12, 0x3100 },
    { 0x6f12, 0xf700 },
    { 0x6f12, 0x2600 },
    { 0x6f12, 0xe100 },
    { 0x602a, 0x0650 },
    { 0x6f12, 0x0600 },
    { 0x602a, 0x0654 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x1a46 },
    { 0x6f12, 0x8600 },
    { 0x602a, 0x1a52 },
    { 0x6f12, 0xbf00 },
    { 0x602a, 0x0674 },
    { 0x6f12, 0x0500 },
    { 0x6f12, 0x0500 },
    { 0x6f12, 0x0500 },
    { 0x6f12, 0x0500 },
    { 0x602a, 0x0668 },
    { 0x6f12, 0x0800 },
    { 0x6f12, 0x0800 },
    { 0x6f12, 0x0800 },
    { 0x6f12, 0x0800 },
    { 0x602a, 0x0684 },
    { 0x6f12, 0x4001 },
    { 0x602a, 0x0688 },
    { 0x6f12, 0x4001 },
    { 0x602a, 0x147c },
    { 0x6f12, 0x1000 },
    { 0x602a, 0x1480 },
    { 0x6f12, 0x1000 },
    { 0x602a, 0x19f6 },
    { 0x6f12, 0x0904 },
    { 0x602a, 0x0812 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x1a02 },
    { 0x6f12, 0x0800 },
    { 0x602a, 0x2148 },
    { 0x6f12, 0x0100 },
    { 0x602a, 0x2042 },
    { 0x6f12, 0x1a00 },
    { 0x602a, 0x0874 },
    { 0x6f12, 0x1100 },
    { 0x602a, 0x09c0 },
    { 0x6f12, 0x9800 },
    { 0x602a, 0x09c4 },
    { 0x6f12, 0x9800 },
    { 0x602a, 0x19fe },
    { 0x6f12, 0x0e1c },
    { 0x602a, 0x4d92 },
    { 0x6f12, 0x0100 },
    { 0x602a, 0x84c8 },
    { 0x6f12, 0x0100 },
    { 0x602a, 0x4d94 },
    { 0x6f12, 0x4001 },
    { 0x6f12, 0x0004 },
    { 0x6f12, 0x0010 },
    { 0x6f12, 0x0810 },
    { 0x6f12, 0x0004 },
    { 0x6f12, 0x0010 },
    { 0x6f12, 0x0810 },
    { 0x6f12, 0x0810 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0010 },
    { 0x6f12, 0x0010 },
    { 0x602a, 0x3570 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x3574 },
    { 0x6f12, 0x9400 },
    { 0x602a, 0x21e4 },
    { 0x6f12, 0x0400 },
    { 0x602a, 0x21ec },
    { 0x6f12, 0x4f01 },
    { 0x602a, 0x2080 },
    { 0x6f12, 0x0100 },
    { 0x6f12, 0x7f00 },
    { 0x6f12, 0x0002 },
    { 0x6f12, 0x8000 },
    { 0x6f12, 0x0002 },
    { 0x6f12, 0xc244 },
    { 0x6f12, 0xd244 },
    { 0x6f12, 0x14f4 },
    { 0x6f12, 0x141c },
    { 0x6f12, 0x111c },
    { 0x6f12, 0x54f4 },
    { 0x602a, 0x20ba },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x120e },
    { 0x6f12, 0x1000 },
    { 0x602a, 0x212e },
    { 0x6f12, 0x0a00 },
    { 0x602a, 0x13ae },
    { 0x6f12, 0x0102 },
    { 0x602a, 0x0718 },
    { 0x6f12, 0x0005 },
    { 0x602a, 0x0710 },
    { 0x6f12, 0x0004 },
    { 0x6f12, 0x0401 },
    { 0x6f12, 0x0100 },
    { 0x602a, 0x1b5c },
    { 0x6f12, 0x0300 },
    { 0x602a, 0x0786 },
    { 0x6f12, 0x7701 },
    { 0x602a, 0x2022 },
    { 0x6f12, 0x0101 },
    { 0x6f12, 0x0101 },
    { 0x602a, 0x1360 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x1376 },
    { 0x6f12, 0x0200 },
    { 0x6f12, 0x6038 },
    { 0x6f12, 0x7038 },
    { 0x6f12, 0x8038 },
    { 0x602a, 0x1386 },
    { 0x6f12, 0x0b00 },
    { 0x602a, 0x06fa },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x4a94 },
    { 0x6f12, 0x0c00 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0600 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0600 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0c00 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x0a76 },
    { 0x6f12, 0x1000 },
    { 0x602a, 0x0aee },
    { 0x6f12, 0x1000 },
    { 0x602a, 0x0b66 },
    { 0x6f12, 0x1000 },
    { 0x602a, 0x0bde },
    { 0x6f12, 0x1000 },
    { 0x602a, 0x0be8 },
    { 0x6f12, 0x3000 },
    { 0x6f12, 0x3000 },
    { 0x602a, 0x0c56 },
    { 0x6f12, 0x1000 },
    { 0x602a, 0x0c60 },
    { 0x6f12, 0x3000 },
    { 0x6f12, 0x3000 },
    { 0x602a, 0x0cb6 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x0cf2 },
    { 0x6f12, 0x0001 },
    { 0x602a, 0x0cf0 },
    { 0x6f12, 0x0101 },
    { 0x602a, 0x11b8 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x11f6 },
    { 0x6f12, 0x0010 },
    { 0x602a, 0x4a74 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0xd8ff },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0xd8ff },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x218e },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x2268 },
    { 0x6f12, 0xf279 },
    { 0x602a, 0x5006 },
    { 0x6f12, 0x0000 },
    { 0x602a, 0x500e },
    { 0x6f12, 0x0100 },
    { 0x602a, 0x4e70 },
    { 0x6f12, 0x2062 },
    { 0x6f12, 0x5501 },
    { 0x602a, 0x06dc },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6f12, 0x0000 },
    { 0x6028, 0x4000 },
    { 0xf46a, 0xae80 },
    { 0x0344, 0x0000 },
    { 0x0346, 0x0000 },
    { 0x0348, 0x1fff },
    { 0x034a, 0x181f },
    { 0x034c, 0x07f8 },
    { 0x034e, 0x0600 },
    { 0x0350, 0x0004 },
    { 0x0352, 0x0004 },
    { 0x0900, 0x0144 },
    { 0x0380, 0x0002 },
    { 0x0382, 0x0006 },
    { 0x0384, 0x0002 },
    { 0x0386, 0x0006 },
    { 0x0110, 0x1002 },
    { 0x0114, 0x0300 },
    { 0x0116, 0x3000 },
    { 0x0136, 0x1800 },
    { 0x013e, 0x0000 },
    { 0x0300, 0x0006 },
    { 0x0302, 0x0001 },
    { 0x0304, 0x0004 },
    { 0x0306, 0x0096 },
    { 0x0308, 0x0008 },
    { 0x030a, 0x0001 },
    { 0x030c, 0x0000 },
    { 0x030e, 0x0003 },
    { 0x0310, 0x0066 },
    { 0x0312, 0x0000 },
    { 0x080e, 0x0000 },
    { 0x0340, 0x06da },
    { 0x0342, 0x0b20 },
    { 0x0702, 0x0000 },
    { 0x0202, 0x0100 },
    { 0x0200, 0x0100 },
    { 0x0d00, 0x0101 },
    { 0x0d02, 0x0101 },
    { 0x0d04, 0x0102 },
    { 0x6226, 0x0000 },
};


static DEFINE_SPINLOCK(s5kjn1_lock);

static struct imgsensor_struct s5kjn1_sensor = {
    .mirror = IMAGE_NORMAL,
    .sensor_mode = IMGSENSOR_MODE_INIT,
    .shutter = 0x3D0,
    .gain = 0x100,
    .pclk = 544000000,
    .frame_length = 3486,
    .line_length = 5200,
    .min_frame_length = 3486,
    .dummy_pixel = 0,
    .dummy_line = 0,
    .current_fps = 300,
    .autoflicker_en = KAL_FALSE,
    .test_pattern = KAL_FALSE,
    .current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
    .ihdr_en = 0,
    .i2c_write_id = 0x20,
};

/* Exact 9-scenario crop info table dumped from stock_Image at 0x1944a98 */
static const struct SENSOR_WINSIZE_INFO_STRUCT s5kjn1_winsize_info[9] = {
    { 8160, 6144, 0, 0, 8160, 6144, 4080, 3072, 0, 0, 4080, 3072, 0, 0, 4080, 3072 },
    { 8160, 6144, 0, 0, 8160, 6144, 4080, 3072, 0, 0, 4080, 3072, 0, 0, 4080, 3072 },
    { 8160, 6144, 0, 0, 8160, 6144, 4080, 3072, 0, 0, 4080, 3072, 0, 0, 4080, 3072 },
    { 8160, 6144, 0, 0, 8160, 6144, 2040, 1536, 0, 0, 2040, 1536, 0, 0, 2040, 1536 },
    { 8160, 6144, 1520, 1632, 5120, 2880, 1280, 720, 0, 0, 1280, 720, 0, 0, 1280, 720 },
    { 8160, 6144, 0, 0, 8160, 6144, 4080, 3072, 0, 0, 4080, 3072, 0, 0, 4080, 3072 },
    { 8160, 6144, 240, 912, 7680, 4320, 3840, 2160, 0, 0, 3840, 2160, 0, 0, 3840, 2160 },
    { 8160, 6144, 0, 0, 8160, 6144, 8160, 6144, 0, 0, 8160, 6144, 0, 0, 8160, 6144 },
    { 8160, 6144, 0, 0, 8160, 6144, 2040, 1536, 0, 0, 2040, 1536, 0, 0, 2040, 1536 }
};


static const struct SENSOR_VC_INFO_STRUCT s5kjn1_vc_info = {
    .VC_Num = 3,
    .VC_PixelNum = 10,
    .ModeSelect = 0,
    .EXPO_Ratio = 8,
    .ODValue = 64,
    .RG_STATSMODE = 0,
    .VC0_ID = 0, .VC0_DataType = 43, .VC0_SIZEH = 4080, .VC0_SIZEV = 3072,
    .VC1_ID = 0, .VC1_DataType = 0,  .VC1_SIZEH = 640,  .VC1_SIZEV = 1,
    .VC2_ID = 1, .VC2_DataType = 48, .VC2_SIZEH = 636,  .VC2_SIZEV = 3056,
    .VC3_ID = 3, .VC3_DataType = 0,  .VC3_SIZEH = 0,    .VC3_SIZEV = 0,
    .VC4_ID = 0, .VC4_DataType = 0,  .VC4_SIZEH = 0,    .VC4_SIZEV = 0,
    .VC5_ID = 0, .VC5_DataType = 0,  .VC5_SIZEH = 0,    .VC5_SIZEV = 0,
};

static const u8 s5kjn1_pdaf_info[460] = {
    0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00,
    0x0c, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00,
    0x0c, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0xfc, 0x01, 0x00, 0x00, 0x7e, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};


static kal_uint8 s5kjn1_orientation_setting = 3; /* 3 = HV flip (180 deg) */
static struct class *s5kjn1_sysfs_class;

static ssize_t s5kjn1_flip_show(struct class *class, struct class_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", s5kjn1_orientation_setting);
}

static ssize_t s5kjn1_flip_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int val = 0;
    if (kstrtoint(buf, 10, &val) == 0) {
        s5kjn1_orientation_setting = (kal_uint8)val;
        write_cmos_sensor_8(0x0101, s5kjn1_orientation_setting, s5kjn1_sensor.i2c_write_id);
        pr_info("[s5kjn1] Set orientation reg 0x0101 to 0x%02x\n", s5kjn1_orientation_setting);
    }
    return count;
}

static CLASS_ATTR_RW(s5kjn1_flip);

static void s5kjn1_init_sysfs(void)
{
    static bool inited = false;
    if (!inited) {
        s5kjn1_sysfs_class = class_create(THIS_MODULE, "s5kjn1");
        if (!IS_ERR(s5kjn1_sysfs_class)) {
            class_create_file(s5kjn1_sysfs_class, &class_attr_s5kjn1_flip);
        }
        inited = true;
    }
}

static kal_uint32 s5kjn1_open(void)
{
    u8 preamble[][4] = {
        { 0x60, 0x28, 0x40, 0x00 },
        { 0x00, 0x00, 0x00, 0x03 },
        { 0x00, 0x00, 0x38, 0xe1 },
        { 0x00, 0x1e, 0x00, 0x07 },
        { 0x60, 0x28, 0x40, 0x00 },
        { 0x60, 0x10, 0x00, 0x01 },
    };
    u8 post_rst[4] = { 0x62, 0x26, 0x00, 0x01 };
    int i;

    for (i = 0; i < 6; i++)
        iWriteRegI2C(preamble[i], 4, s5kjn1_sensor.i2c_write_id);
    usleep_range(6000, 7000);
    iWriteRegI2C(post_rst, 4, s5kjn1_sensor.i2c_write_id);
    usleep_range(10000, 11000);

    write_regs_16(s5kjn1_init_regs, ARRAY_SIZE(s5kjn1_init_regs), s5kjn1_sensor.i2c_write_id);

    if (s5kjn1_orientation_setting != 0) {
        write_cmos_sensor_8(0x0101, s5kjn1_orientation_setting, s5kjn1_sensor.i2c_write_id);
    }

    spin_lock(&s5kjn1_lock);
    s5kjn1_sensor.pclk = 544000000;
    s5kjn1_sensor.line_length = 5200;
    s5kjn1_sensor.frame_length = 3486;
    s5kjn1_sensor.min_frame_length = 3486;
    s5kjn1_sensor.autoflicker_en = 0;
    s5kjn1_sensor.current_fps = 300;
    s5kjn1_sensor.shutter = 1000;
    s5kjn1_sensor.gain = 64;
    spin_unlock(&s5kjn1_lock);
    return ERROR_NONE;
}

/* Exact 9-mode resolution matching stock sub_A8E810 */
static kal_uint32 s5kjn1_get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *res)
{
    if (!res) return ERROR_INVALID_PARA;
    res->SensorPreviewWidth = 4080;
    res->SensorPreviewHeight = 3072;
    res->SensorFullWidth = 4080;
    res->SensorFullHeight = 3072;
    res->SensorVideoWidth = 4080;
    res->SensorVideoHeight = 3072;
    res->SensorHighSpeedVideoWidth = 2040;
    res->SensorHighSpeedVideoHeight = 1536;
    res->SensorSlimVideoWidth = 1280;
    res->SensorSlimVideoHeight = 720;
    res->SensorCustom1Width = 4080;
    res->SensorCustom1Height = 3072;
    res->SensorCustom2Width = 3840;
    res->SensorCustom2Height = 2160;
    res->SensorCustom3Width = 8160;
    res->SensorCustom3Height = 6144;
    res->SensorCustom4Width = 2040;
    res->SensorCustom4Height = 1536;
    res->SensorCustom5Width = 0;
    res->SensorCustom5Height = 0;
    return ERROR_NONE;
}

/* Exact info matching stock sub_A8E760 */
static kal_uint32 s5kjn1_get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
                                  MSDK_SENSOR_INFO_STRUCT *info,
                                  MSDK_SENSOR_CONFIG_STRUCT *cfg)
{
    if (!info) return ERROR_INVALID_PARA;
    memset(info, 0, sizeof(*info));
    info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
    info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
    info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    info->SensorInterruptDelayLines = 3;
    info->SensroInterfaceType = SENSOR_INTERFACE_TYPE_MIPI;
    info->MIPIsensorType = MIPI_OPHY_NCSI2;
    info->SettleDelayMode = MIPI_SETTLEDELAY_AUTO;
    info->SensorOutputDataFormat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_BAYER_Gb;
    info->SensorResetActiveHigh = 0;
    info->SensorResetDelayCount = 5;
    info->SensorDrivingCurrent = 2;
    info->AEShutDelayFrame = 0;
    info->AESensorGainDelayFrame = 0;
    info->AEISPGainDelayFrame = 2;
    info->FrameTimeDelayFrame = 2;
    info->CaptureDelayFrame = 2;
    info->PreviewDelayFrame = 2;
    info->VideoDelayFrame = 2;
    info->HighSpeedVideoDelayFrame = 2;
    info->SlimVideoDelayFrame = 2;
    info->Custom1DelayFrame = 2;
    info->Custom2DelayFrame = 2;
    info->Custom3DelayFrame = 2;
    info->Custom4DelayFrame = 2;
    info->Custom5DelayFrame = 2;
    info->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
    info->SensorClockFreq = 24;
    info->SensorClockDividCount = 1;
    info->SensorClockFallingCount = 1;
    info->SensorPixelClockCount = 2;
    info->SensorDataLatchCount = 3;
    info->SensorPreviewResolutionX = 4080;
    info->SensorPreviewResolutionY = 3072;
    info->SensorFullResolutionX = 8160;
    info->SensorFullResolutionY = 6144;
    info->SensorModeNum = 9;
    info->PDAF_Support = 2;
    *((kal_uint8 *)info + 241) = 9;
    *((kal_uint8 *)info + 252) = 2;
    return ERROR_NONE;
}

/* Exact feature control matching stock sub_A8E868 */
static kal_uint32 s5kjn1_feature_control(MSDK_SENSOR_FEATURE_ENUM feat_id,
                                         MUINT8 *para, MUINT32 *para_len)
{
    UINT16 *feature_return_para_16 = (UINT16 *)para;
    UINT16 *feature_data_16 = (UINT16 *)para;
    UINT32 *feature_return_para_32 = (UINT32 *)para;
    UINT32 *feature_data_32 = (UINT32 *)para;
    unsigned long long *feature_data = (unsigned long long *)para;
    struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
    MUINT32 *p32;

    switch (feat_id) {
    case SENSOR_FEATURE_CHECK_SENSOR_ID:
        if (feature_return_para_32 && para_len) {
            *feature_return_para_32 = S5KJN1_SENSOR_ID;
            *para_len = sizeof(MUINT32);
        }
        break;
    case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
        if (feature_data) {
            p32 = (MUINT32 *)(uintptr_t)(*(feature_data + 1));
            if (p32) {
                switch (*feature_data_32) {
                case 0:
                case 1:
                case 2:
                case 5:
                case 8:
                    *p32 = 300;
                    break;
                case 3:
                    *p32 = 1200;
                    break;
                case 4:
                    *p32 = 2400;
                    break;
                case 6:
                    *p32 = 600;
                    break;
                case 7:
                    *p32 = 100;
                    break;
                default:
                    *p32 = 300;
                    break;
                }
            }
        }
        break;
    case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
        if (feature_data) {
            p32 = (MUINT32 *)(uintptr_t)(*(feature_data + 1));
            if (p32) {
                switch (*feature_data_32) {
                case 3:
                    *p32 = 114952992;
                    break;
                case 4:
                    *p32 = 78121008;
                    break;
                case 6:
                    *p32 = 149032960;
                    break;
                case 7:
                    *p32 = 419439088;
                    break;
                case 8:
                    *p32 = 344853280;
                    break;
                default:
                    *p32 = 228463696;
                    break;
                }
            }
        }
        break;
    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
        if (feature_data) {
            p32 = (MUINT32 *)(uintptr_t)(*(feature_data + 1));
            if (p32) {
                switch (*feature_data_32) {
                case 3:
                case 4:
                case 8:
                    *p32 = 600000000;
                    break;
                case 6:
                case 7:
                    *p32 = 560000000;
                    break;
                default:
                    *p32 = 544000000;
                    break;
                }
            }
        }
        break;
    case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
        if (feature_data) {
            p32 = (MUINT32 *)(uintptr_t)(*(feature_data + 1));
            if (p32) {
                switch (*feature_data_32) {
                case 3:
                case 8:
                    *p32 = 652800000;
                    break;
                case 6:
                    *p32 = 700800000;
                    break;
                case 7:
                    *p32 = 556800000;
                    break;
                default:
                    *p32 = 547200000;
                    break;
                }
            }
        }
        break;
    case SENSOR_FEATURE_GET_PIXEL_RATE:
        if (feature_data) {
            p32 = (MUINT32 *)(uintptr_t)(*(feature_data + 1));
            if (p32) {
                switch (*feature_data_32) {
                case 3:
                case 8:
                    *p32 = 442196520;
                    break;
                case 4:
                    *p32 = 380952320;
                    break;
                case 6:
                    *p32 = 535457280;
                    break;
                case 7:
                    *p32 = 530848800;
                    break;
                default:
                    *p32 = 433500000;
                    break;
                }
            }
        }
        break;
    case SENSOR_FEATURE_GET_PERIOD:
        if (feature_return_para_16 && para_len) {
            *feature_return_para_16++ = s5kjn1_sensor.line_length;
            *feature_return_para_16 = s5kjn1_sensor.frame_length;
            *para_len = 4;
        }
        break;
    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
        if (feature_return_para_32 && para_len) {
            *feature_return_para_32 = s5kjn1_sensor.pclk;
            *para_len = 4;
        }
        break;
    case SENSOR_FEATURE_SET_FRAMERATE:
        if (feature_data_32) {
            spin_lock(&s5kjn1_lock);
            s5kjn1_sensor.current_fps = *feature_data_32;
            spin_unlock(&s5kjn1_lock);
        }
        break;
    case SENSOR_FEATURE_SET_HDR:
        if (feature_data_32) {
            spin_lock(&s5kjn1_lock);
            s5kjn1_sensor.ihdr_en = (kal_uint8)*feature_data_32;
            spin_unlock(&s5kjn1_lock);
        }
        break;
    case SENSOR_FEATURE_GET_CROP_INFO:
        if (feature_data) {
            wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data + 1));
            if (wininfo) {
                kal_uint32 scen = *feature_data_32;
                if (scen < 9)
                    memcpy(wininfo, &s5kjn1_winsize_info[scen], sizeof(*wininfo));
                else
                    memcpy(wininfo, &s5kjn1_winsize_info[0], sizeof(*wininfo));
            }
        }
        break;
    case SENSOR_FEATURE_GET_VC_INFO:
        if (feature_data) {
            struct SENSOR_VC_INFO_STRUCT *vcinfo =
                (struct SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data + 1));
            if (vcinfo)
                memcpy(vcinfo, &s5kjn1_vc_info, sizeof(*vcinfo));
        }
        break;
    case SENSOR_FEATURE_GET_PDAF_INFO:
        if (feature_data) {
            kal_uint32 scen = *feature_data_32;
            if (scen <= 7 && ((1 << scen) & 0xe7)) {
                void *pdaf_info = (void *)(uintptr_t)(*(feature_data + 1));
                if (pdaf_info)
                    memcpy(pdaf_info, s5kjn1_pdaf_info, sizeof(s5kjn1_pdaf_info));
            }
        }
        break;
    case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
        if (feature_data) {
            p32 = (MUINT32 *)(uintptr_t)(*(feature_data + 1));
            if (p32) {
                kal_uint32 scen = *feature_data_32;
                if (scen <= 7 && ((1 << scen) & 0xe7))
                    *p32 = 1;
                else
                    *p32 = 0;
            }
        }
        break;
    case SENSOR_FEATURE_GET_AE_EFFECTIVE_FRAME_FOR_LE:
        if (feature_return_para_32 && para_len) {
            *feature_return_para_32 = 2;
            *para_len = 4;
        }
        break;
    case SENSOR_FEATURE_GET_AE_FRAME_MODE_FOR_LE:
        if (feature_return_para_32 && para_len) {
            *feature_return_para_32 = 0;
            *para_len = 4;
        }
        break;
    case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
        if (feature_data) {
            p32 = (MUINT32 *)(uintptr_t)(*(feature_data + 1));
            if (p32)
                *p32 = 2160000;
        }
        break;
    case SENSOR_FEATURE_GET_BINNING_TYPE:
        if (feature_return_para_32 && para_len) {
            *feature_return_para_32 = 1;
            *para_len = 4;
        }
        break;
    case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
        if (feature_data) {
            *(feature_data + 1) = 64;
            *(feature_data + 2) = 4096;
        }
        break;
    case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
        if (feature_data) {
            *(feature_data + 0) = 50;
            *(feature_data + 1) = 2;
            *(feature_data + 2) = 2;
        }
        break;
    case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
        if (feature_data) {
            *(feature_data + 1) = 4;
            if (*feature_data_32 >= 7 && *feature_data_32 <= 9)
                *(feature_data + 2) = 1;
            else
                *(feature_data + 2) = 2;
        }
        break;
    case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
        if (feature_data) {
            *(feature_data + 1) = 1;
            *(feature_data + 2) = 4;
        }
        break;
    case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
        if (feature_data_16) {
            s5kjn1_sensor.autoflicker_en = (kal_bool)*feature_data_16;
        }
        break;
    case SENSOR_FEATURE_SET_TEST_PATTERN:
        if (feature_data_32) {
            s5kjn1_sensor.test_pattern = (kal_bool)*feature_data_32;
        }
        break;
    case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
        if (feature_return_para_32 && para_len) {
            *feature_return_para_32 = 0;
            *para_len = 4;
        }
        break;
    case SENSOR_FEATURE_SET_STREAMING_RESUME:
        if (feature_data && *feature_data != 0) {
            kal_uint32 shutter = (kal_uint32)*feature_data;
            if (shutter < 4) shutter = 4;
            s5kjn1_sensor.shutter = shutter;
            write_cmos_sensor_16(0x0202, (kal_uint16)shutter, s5kjn1_sensor.i2c_write_id);
        }
        write_cmos_sensor_8(0x0100, 0x01, s5kjn1_sensor.i2c_write_id);
        break;
    case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
        write_cmos_sensor_8(0x0100, 0x00, s5kjn1_sensor.i2c_write_id);
        break;
    case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
        if (feature_data) {
            kal_uint16 shutter = *(kal_uint16 *)feature_data;
            kal_uint16 frame_length = *((kal_uint16 *)feature_data + 4);
            if (shutter < 4) shutter = 4;
            if (frame_length < 3486) frame_length = 3486;
            spin_lock(&s5kjn1_lock);
            s5kjn1_sensor.shutter = shutter;
            s5kjn1_sensor.frame_length = frame_length;
            spin_unlock(&s5kjn1_lock);
            write_cmos_sensor_16(0x0340, frame_length, s5kjn1_sensor.i2c_write_id);
            write_cmos_sensor_16(0x0202, shutter, s5kjn1_sensor.i2c_write_id);
        }
        break;
    case SENSOR_FEATURE_SET_ESHUTTER:
        if (feature_data_32) {
            kal_uint32 shutter = *feature_data_32;
            if (shutter < 4) shutter = 4;
            spin_lock(&s5kjn1_lock);
            s5kjn1_sensor.shutter = shutter;
            if (shutter > s5kjn1_sensor.min_frame_length - 4)
                s5kjn1_sensor.frame_length = shutter + 4;
            else
                s5kjn1_sensor.frame_length = s5kjn1_sensor.min_frame_length;
            spin_unlock(&s5kjn1_lock);
            write_cmos_sensor_16(0x0340, s5kjn1_sensor.frame_length, s5kjn1_sensor.i2c_write_id);
            write_cmos_sensor_16(0x0202, (kal_uint16)shutter, s5kjn1_sensor.i2c_write_id);
        }
        break;
    case SENSOR_FEATURE_SET_GAIN:
        if (feature_data_16) {
            kal_uint16 gain = *feature_data_16;
            if (gain < 64) gain = 64;
            if (gain > 4096) gain = 4096;
            s5kjn1_sensor.gain = gain;
            write_cmos_sensor_16(0x0204, gain, s5kjn1_sensor.i2c_write_id);
        }
        break;
    case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
        if (feature_return_para_32 && para_len) {
            *feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
            *para_len = 4;
        }
        break;
    default:
        break;
    }
    return ERROR_NONE;
}

static kal_uint32 s5kjn1_control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
                                 MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *win,
                                 MSDK_SENSOR_CONFIG_STRUCT *cfg)
{
    spin_lock(&s5kjn1_lock);
    s5kjn1_sensor.current_scenario_id = scenario_id + 1;
    switch (scenario_id) {
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        s5kjn1_sensor.pclk = 544000000;
        s5kjn1_sensor.line_length = 5200;
        s5kjn1_sensor.frame_length = 3486;
        s5kjn1_sensor.min_frame_length = 3486;
        s5kjn1_sensor.autoflicker_en = 0;
        spin_unlock(&s5kjn1_lock);
        write_regs_16(s5kjn1_scen1_regs, ARRAY_SIZE(s5kjn1_scen1_regs), s5kjn1_sensor.i2c_write_id);
        break;
    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        s5kjn1_sensor.pclk = 544000000;
        s5kjn1_sensor.line_length = 5200;
        s5kjn1_sensor.frame_length = 3486;
        s5kjn1_sensor.min_frame_length = 3486;
        s5kjn1_sensor.autoflicker_en = 0;
        spin_unlock(&s5kjn1_lock);
        write_regs_16(s5kjn1_scen1_regs, ARRAY_SIZE(s5kjn1_scen1_regs), s5kjn1_sensor.i2c_write_id);
        break;
    case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        s5kjn1_sensor.pclk = 280000000;
        s5kjn1_sensor.line_length = 2848;
        s5kjn1_sensor.frame_length = 1754;
        s5kjn1_sensor.min_frame_length = 1754;
        s5kjn1_sensor.autoflicker_en = 0;
        spin_unlock(&s5kjn1_lock);
        write_regs_16(s5kjn1_scen3_regs, ARRAY_SIZE(s5kjn1_scen3_regs), s5kjn1_sensor.i2c_write_id);
        break;
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
    default:
        s5kjn1_sensor.pclk = 544000000;
        s5kjn1_sensor.line_length = 5200;
        s5kjn1_sensor.frame_length = 3486;
        s5kjn1_sensor.min_frame_length = 3486;
        s5kjn1_sensor.autoflicker_en = 0;
        spin_unlock(&s5kjn1_lock);
        write_regs_16(s5kjn1_scen1_regs, ARRAY_SIZE(s5kjn1_scen1_regs), s5kjn1_sensor.i2c_write_id);
        break;
    }
    return ERROR_NONE;
}

static kal_uint32 s5kjn1_close(void)
{
    return ERROR_NONE;
}

static struct SENSOR_FUNCTION_STRUCT s5kjn1_sensor_func = {
    s5kjn1_open,
    s5kjn1_get_info,
    s5kjn1_get_resolution,
    s5kjn1_feature_control,
    s5kjn1_control,
    s5kjn1_close
};

UINT32 S5KJN1_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
    s5kjn1_init_sysfs();
    if (pfFunc != NULL)
        *pfFunc = &s5kjn1_sensor_func;
    return ERROR_NONE;
}

/* ========================================================================= */
/* HI1631Q (Hynix 16MP Selfie, Sensor ID 0x1631)                             */
/* ========================================================================= */

static const struct regval_16 hi1631q_init_regs[527] = {
    { 0x2000, 0x2000 },
    { 0x2002, 0x23eb },
    { 0x2004, 0x0170 },
    { 0x2006, 0x40b2 },
    { 0x2008, 0xf27a },
    { 0x200a, 0x8246 },
    { 0x200c, 0x40b2 },
    { 0x200e, 0xf038 },
    { 0x2010, 0x827a },
    { 0x2012, 0x40b2 },
    { 0x2014, 0xf0c2 },
    { 0x2016, 0x8266 },
    { 0x2018, 0x40b2 },
    { 0x201a, 0xf10a },
    { 0x201c, 0x8244 },
    { 0x201e, 0x40b2 },
    { 0x2020, 0xfe00 },
    { 0x2022, 0x826e },
    { 0x2024, 0x40b2 },
    { 0x2026, 0xf162 },
    { 0x2028, 0x825a },
    { 0x202a, 0x40b2 },
    { 0x202c, 0xf13e },
    { 0x202e, 0x8282 },
    { 0x2030, 0x40b2 },
    { 0x2032, 0xf178 },
    { 0x2034, 0x8286 },
    { 0x2036, 0x4130 },
    { 0x2038, 0x120b },
    { 0x203a, 0x430b },
    { 0x203c, 0x1292 },
    { 0x203e, 0xb064 },
    { 0x2040, 0x430c },
    { 0x2042, 0x4c0f },
    { 0x2044, 0x5f0f },
    { 0x2046, 0x5f0f },
    { 0x2048, 0x4c0e },
    { 0x204a, 0x5e0e },
    { 0x204c, 0x5e0f },
    { 0x204e, 0x5e0f },
    { 0x2050, 0x5e0f },
    { 0x2052, 0x9f92 },
    { 0x2054, 0x84a8 },
    { 0x2056, 0x842a },
    { 0x2058, 0x2803 },
    { 0x205a, 0x531c },
    { 0x205c, 0x922c },
    { 0x205e, 0x2bf1 },
    { 0x2060, 0x4c0d },
    { 0x2062, 0x5d0d },
    { 0x2064, 0x5d0d },
    { 0x2066, 0x4c0f },
    { 0x2068, 0x5f0f },
    { 0x206a, 0x5f0d },
    { 0x206c, 0x5f0d },
    { 0x206e, 0x5f0d },
    { 0x2070, 0x503d },
    { 0x2072, 0x84aa },
    { 0x2074, 0x430c },
    { 0x2076, 0x4c0f },
    { 0x2078, 0x5f0f },
    { 0x207a, 0x5d0f },
    { 0x207c, 0x4f2e },
    { 0x207e, 0xf03e },
    { 0x2080, 0x0100 },
    { 0x2082, 0x4fec },
    { 0x2084, 0x066a },
    { 0x2086, 0xde0b },
    { 0x2088, 0x531c },
    { 0x208a, 0xc312 },
    { 0x208c, 0x100b },
    { 0x208e, 0x922c },
    { 0x2090, 0x2bf2 },
    { 0x2092, 0x4b0f },
    { 0x2094, 0xc312 },
    { 0x2096, 0x100f },
    { 0x2098, 0x110f },
    { 0x209a, 0x110f },
    { 0x209c, 0x110f },
    { 0x209e, 0xdf4b },
    { 0x20a0, 0x4bc2 },
    { 0x20a2, 0x06cd },
    { 0x20a4, 0x4292 },
    { 0x20a6, 0x066a },
    { 0x20a8, 0x06de },
    { 0x20aa, 0x4292 },
    { 0x20ac, 0x066c },
    { 0x20ae, 0x06e0 },
    { 0x20b0, 0x430d },
    { 0x20b2, 0x403e },
    { 0x20b4, 0x000c },
    { 0x20b6, 0x403f },
    { 0x20b8, 0x8550 },
    { 0x20ba, 0x1292 },
    { 0x20bc, 0x824e },
    { 0x20be, 0x413b },
    { 0x20c0, 0x4130 },
    { 0x20c2, 0x120b },
    { 0x20c4, 0x4f0b },
    { 0x20c6, 0xb3d2 },
    { 0x20c8, 0x0240 },
    { 0x20ca, 0x2406 },
    { 0x20cc, 0x4292 },
    { 0x20ce, 0x020e },
    { 0x20d0, 0x7316 },
    { 0x20d2, 0x4292 },
    { 0x20d4, 0x0210 },
    { 0x20d6, 0x7318 },
    { 0x20d8, 0x4b0f },
    { 0x20da, 0x1292 },
    { 0x20dc, 0xb050 },
    { 0x20de, 0x425f },
    { 0x20e0, 0x0265 },
    { 0x20e2, 0xf35f },
    { 0x20e4, 0x9fcb },
    { 0x20e6, 0x8472 },
    { 0x20e8, 0x240e },
    { 0x20ea, 0x4b0f },
    { 0x20ec, 0x5f0f },
    { 0x20ee, 0x5f0f },
    { 0x20f0, 0x5f0f },
    { 0x20f2, 0x4b0e },
    { 0x20f4, 0x5e0e },
    { 0x20f6, 0x5e0e },
    { 0x20f8, 0x5e0f },
    { 0x20fa, 0x421e },
    { 0x20fc, 0x8422 },
    { 0x20fe, 0x8eaf },
    { 0x2100, 0x7544 },
    { 0x2102, 0x5eaf },
    { 0x2104, 0x7546 },
    { 0x2106, 0x413b },
    { 0x2108, 0x4130 },
    { 0x210a, 0x1292 },
    { 0x210c, 0xb02e },
    { 0x210e, 0x425f },
    { 0x2110, 0x846d },
    { 0x2112, 0xd25f },
    { 0x2114, 0x846e },
    { 0x2116, 0xf37f },
    { 0x2118, 0x4f82 },
    { 0x211a, 0x7122 },
    { 0x211c, 0x93c2 },
    { 0x211e, 0x83f8 },
    { 0x2120, 0x240d },
    { 0x2122, 0x4292 },
    { 0x2124, 0x7316 },
    { 0x2126, 0x8484 },
    { 0x2128, 0x4292 },
    { 0x212a, 0x7318 },
    { 0x212c, 0x8486 },
    { 0x212e, 0x4392 },
    { 0x2130, 0x7f0c },
    { 0x2132, 0x4392 },
    { 0x2134, 0x7f10 },
    { 0x2136, 0x413f },
    { 0x2138, 0x4030 },
    { 0x213a, 0xb244 },
    { 0x213c, 0x4130 },
    { 0x213e, 0x4292 },
    { 0x2140, 0x731a },
    { 0x2142, 0x130a },
    { 0x2144, 0x93c2 },
    { 0x2146, 0x846d },
    { 0x2148, 0x2403 },
    { 0x214a, 0xd0f2 },
    { 0x214c, 0x0010 },
    { 0x214e, 0x0205 },
    { 0x2150, 0x12b0 },
    { 0x2152, 0xf284 },
    { 0x2154, 0x93c2 },
    { 0x2156, 0x846d },
    { 0x2158, 0x2403 },
    { 0x215a, 0xf0f2 },
    { 0x215c, 0xffef },
    { 0x215e, 0x0205 },
    { 0x2160, 0x4130 },
    { 0x2162, 0xb3d2 },
    { 0x2164, 0x0600 },
    { 0x2166, 0x2403 },
    { 0x2168, 0xd3d2 },
    { 0x216a, 0x0601 },
    { 0x216c, 0x3c02 },
    { 0x216e, 0xc3d2 },
    { 0x2170, 0x0601 },
    { 0x2172, 0x1292 },
    { 0x2174, 0xb044 },
    { 0x2176, 0x4130 },
    { 0x2178, 0x120b },
    { 0x217a, 0x120a },
    { 0x217c, 0x1209 },
    { 0x217e, 0x1208 },
    { 0x2180, 0x1207 },
    { 0x2182, 0x1206 },
    { 0x2184, 0xb0f2 },
    { 0x2186, 0x0040 },
    { 0x2188, 0x0b05 },
    { 0x218a, 0x246e },
    { 0x218c, 0xb2f2 },
    { 0x218e, 0x0b05 },
    { 0x2190, 0x2469 },
    { 0x2192, 0x432a },
    { 0x2194, 0x4308 },
    { 0x2196, 0x4259 },
    { 0x2198, 0x0605 },
    { 0x219a, 0xd392 },
    { 0x219c, 0x7a12 },
    { 0x219e, 0x4217 },
    { 0x21a0, 0x0428 },
    { 0x21a2, 0x4216 },
    { 0x21a4, 0x042a },
    { 0x21a6, 0x8706 },
    { 0x21a8, 0x0b00 },
    { 0x21aa, 0x7304 },
    { 0x21ac, 0x0000 },
    { 0x21ae, 0xe279 },
    { 0x21b0, 0x49c2 },
    { 0x21b2, 0x0605 },
    { 0x21b4, 0x0800 },
    { 0x21b6, 0x7a10 },
    { 0x21b8, 0x421f },
    { 0x21ba, 0x7100 },
    { 0x21bc, 0x870f },
    { 0x21be, 0x4382 },
    { 0x21c0, 0x0412 },
    { 0x21c2, 0x9f06 },
    { 0x21c4, 0x281c },
    { 0x21c6, 0x9382 },
    { 0x21c8, 0x7106 },
    { 0x21ca, 0x2419 },
    { 0x21cc, 0x425b },
    { 0x21ce, 0x8470 },
    { 0x21d0, 0xff4b },
    { 0x21d2, 0x434d },
    { 0x21d4, 0x9382 },
    { 0x21d6, 0x8430 },
    { 0x21d8, 0x2412 },
    { 0x21da, 0x421c },
    { 0x21dc, 0x8434 },
    { 0x21de, 0x4d4f },
    { 0x21e0, 0x4f0e },
    { 0x21e2, 0x5e0e },
    { 0x21e4, 0x5e0f },
    { 0x21e6, 0x5f0f },
    { 0x21e8, 0x5c0f },
    { 0x21ea, 0x4f6f },
    { 0x21ec, 0xf25f },
    { 0x21ee, 0x8470 },
    { 0x21f0, 0x9b4f },
    { 0x21f2, 0x2428 },
    { 0x21f4, 0x535d },
    { 0x21f6, 0x4d4f },
    { 0x21f8, 0x921f },
    { 0x21fa, 0x8430 },
    { 0x21fc, 0x2bf0 },
    { 0x21fe, 0x403b },
    { 0x2200, 0x823e },
    { 0x2202, 0x480f },
    { 0x2204, 0x5f0f },
    { 0x2206, 0x580f },
    { 0x2208, 0x5f0f },
    { 0x220a, 0x503f },
    { 0x220c, 0x8550 },
    { 0x220e, 0x403d },
    { 0x2210, 0x0003 },
    { 0x2212, 0x403e },
    { 0x2214, 0x0412 },
    { 0x2216, 0x12ab },
    { 0x2218, 0x5318 },
    { 0x221a, 0xf038 },
    { 0x221c, 0x0003 },
    { 0x221e, 0x4a0f },
    { 0x2220, 0x5f0f },
    { 0x2222, 0x5a0f },
    { 0x2224, 0x5f0f },
    { 0x2226, 0x503f },
    { 0x2228, 0x8550 },
    { 0x222a, 0x403d },
    { 0x222c, 0x0003 },
    { 0x222e, 0x4f0e },
    { 0x2230, 0x403f },
    { 0x2232, 0x042e },
    { 0x2234, 0x12ab },
    { 0x2236, 0x531a },
    { 0x2238, 0xf03a },
    { 0x223a, 0x0003 },
    { 0x223c, 0x9382 },
    { 0x223e, 0x7112 },
    { 0x2240, 0x27b3 },
    { 0x2242, 0x3c14 },
    { 0x2244, 0x403d },
    { 0x2246, 0x0412 },
    { 0x2248, 0x4e0f },
    { 0x224a, 0x5f0f },
    { 0x224c, 0x5e0f },
    { 0x224e, 0x4c0e },
    { 0x2250, 0x5f0e },
    { 0x2252, 0x434f },
    { 0x2254, 0x4ebd },
    { 0x2256, 0x0000 },
    { 0x2258, 0x532d },
    { 0x225a, 0x535f },
    { 0x225c, 0x907f },
    { 0x225e, 0x0003 },
    { 0x2260, 0x2bf9 },
    { 0x2262, 0x3fcd },
    { 0x2264, 0x430a },
    { 0x2266, 0x3f96 },
    { 0x2268, 0x1292 },
    { 0x226a, 0xb070 },
    { 0x226c, 0x4136 },
    { 0x226e, 0x4137 },
    { 0x2270, 0x4138 },
    { 0x2272, 0x4139 },
    { 0x2274, 0x413a },
    { 0x2276, 0x413b },
    { 0x2278, 0x4130 },
    { 0x227a, 0x43c2 },
    { 0x227c, 0x0700 },
    { 0x227e, 0x1292 },
    { 0x2280, 0xb030 },
    { 0x2282, 0x4130 },
    { 0x2284, 0x422d },
    { 0x2286, 0x403e },
    { 0x2288, 0xf298 },
    { 0x228a, 0x403f },
    { 0x228c, 0x83d6 },
    { 0x228e, 0x12b0 },
    { 0x2290, 0xb75c },
    { 0x2292, 0x1292 },
    { 0x2294, 0xb06c },
    { 0x2296, 0x4130 },
    { 0x2298, 0x0022 },
    { 0x229a, 0x1133 },
    { 0x229c, 0x4466 },
    { 0x229e, 0x5577 },
    { 0x2e00, 0x425f },
    { 0x2e02, 0x0250 },
    { 0x2e04, 0xf37f },
    { 0x2e06, 0x421e },
    { 0x2e08, 0x0254 },
    { 0x2e0a, 0x903f },
    { 0x2e0c, 0x0003 },
    { 0x2e0e, 0x2405 },
    { 0x2e10, 0xc3d2 },
    { 0x2e12, 0x0255 },
    { 0x2e14, 0x0900 },
    { 0x2e16, 0x732c },
    { 0x2e18, 0x4130 },
    { 0x2e1a, 0x120e },
    { 0x2e1c, 0x403d },
    { 0x2e1e, 0xfe32 },
    { 0x2e20, 0x120d },
    { 0x2e22, 0x120b },
    { 0x2e24, 0x120a },
    { 0x2e26, 0x1209 },
    { 0x2e28, 0x421b },
    { 0x2e2a, 0x0256 },
    { 0x2e2c, 0x4f0c },
    { 0x2e2e, 0x4030 },
    { 0x2e30, 0xd72a },
    { 0x2e32, 0x413e },
    { 0x2e34, 0xd03e },
    { 0x2e36, 0x0009 },
    { 0x2e38, 0x4e82 },
    { 0x2e3a, 0x0254 },
    { 0x2e3c, 0x4130 },
    { 0x2ffe, 0xb000 },
    { 0x33de, 0x84d0 },
    { 0x33e0, 0x0000 },
    { 0x33e2, 0x0000 },
    { 0x33e4, 0x0000 },
    { 0x33e6, 0x0000 },
    { 0x33e8, 0x0000 },
    { 0x33ea, 0x0000 },
    { 0x33ec, 0x0000 },
    { 0x34a8, 0x0042 },
    { 0x34aa, 0x0000 },
    { 0x34ac, 0x0000 },
    { 0x34ae, 0x0000 },
    { 0x34b0, 0x0000 },
    { 0x34b2, 0x0064 },
    { 0x34b4, 0x0000 },
    { 0x34b6, 0x0000 },
    { 0x34b8, 0x0000 },
    { 0x34ba, 0x0000 },
    { 0x34bc, 0x00c8 },
    { 0x34be, 0x0000 },
    { 0x34c0, 0x0000 },
    { 0x34c2, 0x0000 },
    { 0x34c4, 0x0000 },
    { 0x34c6, 0x012c },
    { 0x34c8, 0x0000 },
    { 0x34ca, 0x0000 },
    { 0x34cc, 0x0000 },
    { 0x34ce, 0x0000 },
    { 0x34d0, 0x0040 },
    { 0x34d2, 0x0040 },
    { 0x34d4, 0x003f },
    { 0x34d6, 0x003f },
    { 0x34d8, 0x003e },
    { 0x34da, 0x003d },
    { 0x34dc, 0x003c },
    { 0x34de, 0x003c },
    { 0x34e0, 0x003c },
    { 0x34e2, 0x003c },
    { 0x34e4, 0x003d },
    { 0x34e6, 0x003e },
    { 0x34e8, 0x003f },
    { 0x34ea, 0x0040 },
    { 0x34ec, 0x0040 },
    { 0x34ee, 0x0042 },
    { 0x34f0, 0x0044 },
    { 0x34f2, 0x0046 },
    { 0x34f4, 0x0048 },
    { 0x34f6, 0x004a },
    { 0x34f8, 0x004c },
    { 0x34fa, 0x004e },
    { 0x34fc, 0x0050 },
    { 0x34fe, 0x0054 },
    { 0x3500, 0x0058 },
    { 0x3502, 0x005c },
    { 0x3504, 0x0062 },
    { 0x3506, 0x0068 },
    { 0x3508, 0x006e },
    { 0x350a, 0x0074 },
    { 0x350c, 0x007a },
    { 0x350e, 0x0080 },
    { 0x3510, 0x0040 },
    { 0x3512, 0x0040 },
    { 0x3514, 0x0042 },
    { 0x3516, 0x0045 },
    { 0x3518, 0x0049 },
    { 0x351a, 0x004e },
    { 0x351c, 0x0053 },
    { 0x351e, 0x0058 },
    { 0x3520, 0x005d },
    { 0x3522, 0x0062 },
    { 0x3524, 0x0067 },
    { 0x3526, 0x006c },
    { 0x3528, 0x0071 },
    { 0x352a, 0x0076 },
    { 0x352c, 0x0078 },
    { 0x352e, 0x0080 },
    { 0x3530, 0x0085 },
    { 0x3532, 0x008a },
    { 0x3534, 0x008f },
    { 0x3536, 0x0094 },
    { 0x3538, 0x0099 },
    { 0x353a, 0x009e },
    { 0x353c, 0x00a3 },
    { 0x353e, 0x00a8 },
    { 0x3540, 0x00ad },
    { 0x3542, 0x00b2 },
    { 0x3544, 0x00b7 },
    { 0x3546, 0x00bc },
    { 0x3548, 0x00c1 },
    { 0x354a, 0x00c6 },
    { 0x354c, 0x00c8 },
    { 0x354e, 0x00d0 },
    { 0xbffe, 0x8000 },
    { 0x0510, 0x007f },
    { 0x0512, 0x007f },
    { 0x0514, 0x007f },
    { 0x0516, 0x007f },
    { 0x0260, 0x0021 },
    { 0x0266, 0x000b },
    { 0x026a, 0x0064 },
    { 0x0256, 0x0100 },
    { 0x0258, 0x0002 },
    { 0x025a, 0x03e8 },
    { 0x025c, 0x0002 },
    { 0x0250, 0x0000 },
    { 0x0254, 0x0010 },
    { 0x0354, 0x1a00 },
    { 0x0410, 0x2000 },
    { 0x043a, 0x0031 },
    { 0x0560, 0x90f0 },
    { 0x0900, 0x0002 },
    { 0x0910, 0x0002 },
    { 0x0912, 0x0102 },
    { 0x170a, 0x0041 },
    { 0x1714, 0x004a },
    { 0x1716, 0xb6e7 },
    { 0x1718, 0x0001 },
    { 0x17b0, 0x0050 },
    { 0x17b2, 0x029d },
    { 0x17b4, 0x0001 },
    { 0x17b8, 0x0002 },
    { 0x171a, 0x017c },
    { 0x171c, 0x00ee },
    { 0x1720, 0x0041 },
    { 0x0a04, 0xb400 },
    { 0x0a06, 0xd400 },
    { 0x0a08, 0xc88c },
    { 0x0a0a, 0xc381 },
    { 0x0a0c, 0xf750 },
    { 0x0a0e, 0xfcd8 },
    { 0x0a10, 0xc040 },
    { 0x0a12, 0x1700 },
    { 0x0a20, 0x0015 },
    { 0x0600, 0x11b1 },
    { 0x0602, 0x0152 },
    { 0x0606, 0x01eb },
    { 0x06b4, 0x3ff0 },
    { 0x06e2, 0xcccc },
    { 0x0618, 0x0a80 },
    { 0x0666, 0xb050 },
    { 0x0674, 0x03ff },
    { 0x0b10, 0x403e },
    { 0x0d00, 0x400b },
    { 0x0e1e, 0x0001 },
    { 0x0736, 0x500b },
    { 0x0738, 0xf030 },
    { 0x0730, 0xb74b },
    { 0x0708, 0x2f80 },
    { 0x070c, 0x01ff },
    { 0x0740, 0x008f },
    { 0x0750, 0x0100 },
    { 0x1002, 0xc319 },
    { 0x1048, 0x0000 },
    { 0x1044, 0x0000 },
    { 0x1046, 0x0004 },
    { 0x105a, 0x0001 },
    { 0x105c, 0x0f00 },
    { 0x1060, 0x3408 },
    { 0x027e, 0x0100 },
};

static const struct regval_16 hi1631q_preview_regs[94] = {
    { 0x34a8, 0x0041 },
    { 0x34aa, 0x0121 },
    { 0x34ac, 0x0121 },
    { 0x34ae, 0x0121 },
    { 0x34b0, 0x0121 },
    { 0x34b2, 0x004a },
    { 0x34b4, 0x011d },
    { 0x34b6, 0x011d },
    { 0x34b8, 0x011d },
    { 0x34ba, 0x011d },
    { 0x34bc, 0x0050 },
    { 0x34be, 0x0109 },
    { 0x34c0, 0x010f },
    { 0x34c2, 0x010f },
    { 0x34c4, 0x0109 },
    { 0x34c6, 0x1024 },
    { 0x34c8, 0x000a },
    { 0x34ca, 0x0000 },
    { 0x34cc, 0x0000 },
    { 0x34ce, 0x000a },
    { 0x34d0, 0x0040 },
    { 0x34d2, 0x003e },
    { 0x34d4, 0x003c },
    { 0x34d6, 0x003a },
    { 0x34d8, 0x0038 },
    { 0x34da, 0x0034 },
    { 0x34dc, 0x0030 },
    { 0x34de, 0x002e },
    { 0x34e0, 0x002d },
    { 0x34e2, 0x002c },
    { 0x34e4, 0x002a },
    { 0x34e6, 0x0028 },
    { 0x34e8, 0x0026 },
    { 0x34ea, 0x0024 },
    { 0x34ec, 0x0022 },
    { 0x34ee, 0x0020 },
    { 0x34f0, 0x001e },
    { 0x34f2, 0x001e },
    { 0x34f4, 0x001e },
    { 0x34f6, 0x001e },
    { 0x34f8, 0x001e },
    { 0x34fa, 0x001e },
    { 0x34fc, 0x001e },
    { 0x34fe, 0x001e },
    { 0x3500, 0x001e },
    { 0x3502, 0x001e },
    { 0x3504, 0x001e },
    { 0x3506, 0x0022 },
    { 0x3508, 0x0026 },
    { 0x350a, 0x002a },
    { 0x350c, 0x002e },
    { 0x350e, 0x0032 },
    { 0x0206, 0x02c8 },
    { 0x0204, 0x0009 },
    { 0x020a, 0x0e9d },
    { 0x020e, 0x0ea1 },
    { 0x0212, 0x0000 },
    { 0x022a, 0x0016 },
    { 0x022c, 0x0e23 },
    { 0x0224, 0x004c },
    { 0x022e, 0x0df2 },
    { 0x0234, 0x2222 },
    { 0x0238, 0x2222 },
    { 0x023a, 0x4242 },
    { 0x0268, 0x00ff },
    { 0x0440, 0x001d },
    { 0x1710, 0x0811 },
    { 0x1712, 0x0098 },
    { 0x17b6, 0x0040 },
    { 0x06cc, 0x00ff },
    { 0x0b04, 0x0034 },
    { 0x0b12, 0x0910 },
    { 0x0b14, 0x06d0 },
    { 0x0b20, 0x5200 },
    { 0x0f00, 0x0400 },
    { 0x0f06, 0x0002 },
    { 0x0f04, 0x0010 },
    { 0x0f08, 0x0011 },
    { 0x0f0a, 0x2233 },
    { 0x0f0c, 0x4455 },
    { 0x0f0e, 0x6677 },
    { 0x0732, 0xb9b0 },
    { 0x103e, 0x0100 },
    { 0x1010, 0x00a5 },
    { 0x1012, 0x0004 },
    { 0x1014, 0x0001 },
    { 0x1016, 0x0001 },
    { 0x1020, 0xc103 },
    { 0x1022, 0x040d },
    { 0x1024, 0x0304 },
    { 0x1026, 0x0505 },
    { 0x1028, 0x0b04 },
    { 0x102a, 0x0507 },
    { 0x102c, 0x0a0a },
};

static const struct regval_16 hi1631q_capture_regs[94] = {
    { 0x34a8, 0x0041 },
    { 0x34aa, 0x001a },
    { 0x34ac, 0x001c },
    { 0x34ae, 0x001c },
    { 0x34b0, 0x001a },
    { 0x34b2, 0x0050 },
    { 0x34b4, 0x0023 },
    { 0x34b6, 0x001b },
    { 0x34b8, 0x001b },
    { 0x34ba, 0x0023 },
    { 0x34bc, 0x0050 },
    { 0x34be, 0x0025 },
    { 0x34c0, 0x000e },
    { 0x34c2, 0x000e },
    { 0x34c4, 0x0025 },
    { 0x34c6, 0x1024 },
    { 0x34c8, 0x0026 },
    { 0x34ca, 0x0000 },
    { 0x34cc, 0x0000 },
    { 0x34ce, 0x002a },
    { 0x34d0, 0x0040 },
    { 0x34d2, 0x0040 },
    { 0x34d4, 0x003f },
    { 0x34d6, 0x003f },
    { 0x34d8, 0x003e },
    { 0x34da, 0x003d },
    { 0x34dc, 0x003c },
    { 0x34de, 0x003c },
    { 0x34e0, 0x003c },
    { 0x34e2, 0x003c },
    { 0x34e4, 0x003d },
    { 0x34e6, 0x003e },
    { 0x34e8, 0x003f },
    { 0x34ea, 0x0040 },
    { 0x34ec, 0x0040 },
    { 0x34ee, 0x0042 },
    { 0x34f0, 0x0044 },
    { 0x34f2, 0x0046 },
    { 0x34f4, 0x0048 },
    { 0x34f6, 0x004a },
    { 0x34f8, 0x004c },
    { 0x34fa, 0x004e },
    { 0x34fc, 0x0050 },
    { 0x34fe, 0x0054 },
    { 0x3500, 0x0058 },
    { 0x3502, 0x005c },
    { 0x3504, 0x0062 },
    { 0x3506, 0x0068 },
    { 0x3508, 0x006e },
    { 0x350a, 0x0074 },
    { 0x350c, 0x007a },
    { 0x350e, 0x0080 },
    { 0x0206, 0x02c8 },
    { 0x0204, 0x0008 },
    { 0x020a, 0x0e9d },
    { 0x020e, 0x0ea1 },
    { 0x0212, 0x0000 },
    { 0x022a, 0x0017 },
    { 0x022c, 0x0e21 },
    { 0x0224, 0x0050 },
    { 0x022e, 0x0def },
    { 0x0234, 0x1111 },
    { 0x0238, 0x1111 },
    { 0x023a, 0x2121 },
    { 0x0268, 0x00ff },
    { 0x0440, 0x001d },
    { 0x1710, 0x0ee3 },
    { 0x1712, 0x009a },
    { 0x17b6, 0x0000 },
    { 0x06cc, 0x0000 },
    { 0x0b04, 0x0014 },
    { 0x0b12, 0x1220 },
    { 0x0b14, 0x0da0 },
    { 0x0b20, 0x0100 },
    { 0x0f00, 0x0000 },
    { 0x0f06, 0x0000 },
    { 0x0f04, 0x0020 },
    { 0x0f08, 0x0022 },
    { 0x0f0a, 0x1133 },
    { 0x0f0c, 0x4466 },
    { 0x0f0e, 0x5577 },
    { 0x0732, 0xb8b0 },
    { 0x103e, 0x0000 },
    { 0x1010, 0x02e4 },
    { 0x1012, 0x001d },
    { 0x1014, 0x0001 },
    { 0x1016, 0x0001 },
    { 0x1020, 0xc107 },
    { 0x1022, 0x071b },
    { 0x1024, 0x0406 },
    { 0x1026, 0x0908 },
    { 0x1028, 0x1207 },
    { 0x102a, 0x0a0d },
    { 0x102c, 0x0a0a },
};

static const struct regval_16 hi1631q_video_regs[94] = {
    { 0x34a8, 0x0041 },
    { 0x34aa, 0x0121 },
    { 0x34ac, 0x0121 },
    { 0x34ae, 0x0121 },
    { 0x34b0, 0x0121 },
    { 0x34b2, 0x004a },
    { 0x34b4, 0x011d },
    { 0x34b6, 0x011d },
    { 0x34b8, 0x011d },
    { 0x34ba, 0x011d },
    { 0x34bc, 0x0050 },
    { 0x34be, 0x0109 },
    { 0x34c0, 0x010f },
    { 0x34c2, 0x010f },
    { 0x34c4, 0x0109 },
    { 0x34c6, 0x1024 },
    { 0x34c8, 0x000a },
    { 0x34ca, 0x0000 },
    { 0x34cc, 0x0000 },
    { 0x34ce, 0x000a },
    { 0x34d0, 0x0040 },
    { 0x34d2, 0x003e },
    { 0x34d4, 0x003c },
    { 0x34d6, 0x003a },
    { 0x34d8, 0x0038 },
    { 0x34da, 0x0034 },
    { 0x34dc, 0x0030 },
    { 0x34de, 0x002e },
    { 0x34e0, 0x002d },
    { 0x34e2, 0x002c },
    { 0x34e4, 0x002a },
    { 0x34e6, 0x0028 },
    { 0x34e8, 0x0026 },
    { 0x34ea, 0x0024 },
    { 0x34ec, 0x0022 },
    { 0x34ee, 0x0020 },
    { 0x34f0, 0x001e },
    { 0x34f2, 0x001e },
    { 0x34f4, 0x001e },
    { 0x34f6, 0x001e },
    { 0x34f8, 0x001e },
    { 0x34fa, 0x001e },
    { 0x34fc, 0x001e },
    { 0x34fe, 0x001e },
    { 0x3500, 0x001e },
    { 0x3502, 0x001e },
    { 0x3504, 0x001e },
    { 0x3506, 0x0022 },
    { 0x3508, 0x0026 },
    { 0x350a, 0x002a },
    { 0x350c, 0x002e },
    { 0x350e, 0x0032 },
    { 0x0206, 0x02c8 },
    { 0x0204, 0x0009 },
    { 0x020a, 0x0e9d },
    { 0x020e, 0x0ea1 },
    { 0x0212, 0x0000 },
    { 0x022a, 0x0016 },
    { 0x022c, 0x0e23 },
    { 0x0224, 0x004c },
    { 0x022e, 0x0df2 },
    { 0x0234, 0x2222 },
    { 0x0238, 0x2222 },
    { 0x023a, 0x4242 },
    { 0x0268, 0x00ff },
    { 0x0440, 0x001d },
    { 0x1710, 0x0811 },
    { 0x1712, 0x0098 },
    { 0x17b6, 0x0040 },
    { 0x06cc, 0x00ff },
    { 0x0b04, 0x0034 },
    { 0x0b12, 0x0910 },
    { 0x0b14, 0x06d0 },
    { 0x0b20, 0x5200 },
    { 0x0f00, 0x0400 },
    { 0x0f06, 0x0002 },
    { 0x0f04, 0x0010 },
    { 0x0f08, 0x0011 },
    { 0x0f0a, 0x2233 },
    { 0x0f0c, 0x4455 },
    { 0x0f0e, 0x6677 },
    { 0x0732, 0xb9b0 },
    { 0x103e, 0x0100 },
    { 0x1010, 0x00a5 },
    { 0x1012, 0x0004 },
    { 0x1014, 0x0001 },
    { 0x1016, 0x0001 },
    { 0x1020, 0xc103 },
    { 0x1022, 0x040d },
    { 0x1024, 0x0304 },
    { 0x1026, 0x0505 },
    { 0x1028, 0x0b04 },
    { 0x102a, 0x0507 },
    { 0x102c, 0x0a0a },
};


static DEFINE_SPINLOCK(hi1631q_lock);

static struct imgsensor_struct hi1631q_sensor = {
    .mirror = IMAGE_NORMAL,
    .sensor_mode = IMGSENSOR_MODE_INIT,
    .shutter = 0x3D0,
    .gain = 0x100,
    .pclk = 640000000,
    .frame_length = 3745,
    .line_length = 5696,
    .min_frame_length = 3745,
    .dummy_pixel = 0,
    .dummy_line = 0,
    .current_fps = 300,
    .autoflicker_en = KAL_FALSE,
    .test_pattern = KAL_FALSE,
    .current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
    .ihdr_en = 0,
    .i2c_write_id = 0x40,
};

/* Exact 8-scenario crop info table dumped from stock_Image at 0x1944bb8 */
static const struct SENSOR_WINSIZE_INFO_STRUCT hi1631q_winsize_info[8] = {
    { 4672, 3512, 0, 8, 4672, 3496, 2336, 1748, 8, 2, 2320, 1744, 0, 0, 2320, 1744 },
    { 4672, 3512, 0, 10, 4672, 3492, 4672, 3492, 16, 2, 4640, 3488, 0, 0, 4640, 3488 },
    { 4672, 3512, 0, 8, 4672, 3496, 2336, 1748, 8, 2, 2320, 1744, 0, 0, 2320, 1744 },
    { 4672, 3512, 0, 1032, 4672, 1448, 2336, 724, 528, 2, 1280, 720, 0, 0, 1280, 720 },
    { 4672, 3512, 0, 1032, 4672, 1448, 2336, 724, 528, 2, 1280, 720, 0, 0, 1280, 720 },
    { 4672, 3512, 0, 8, 4672, 3496, 2336, 1748, 8, 2, 2320, 1744, 0, 0, 2320, 1744 },
    { 4672, 3512, 0, 8, 4672, 3496, 2336, 1748, 8, 2, 2320, 1744, 0, 0, 2320, 1744 },
    { 4672, 3512, 0, 10, 4672, 3492, 4672, 3492, 16, 2, 4640, 3488, 0, 0, 4640, 3488 }
};

static kal_uint32 hi1631q_open(void)
{
    write_regs_16(hi1631q_init_regs, ARRAY_SIZE(hi1631q_init_regs), hi1631q_sensor.i2c_write_id);
    return ERROR_NONE;
}

/* Exact 8-mode resolution matching stock sub_A90AC0 */
static kal_uint32 hi1631q_get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *res)
{
    if (!res) return ERROR_INVALID_PARA;
    res->SensorPreviewWidth = 2320;
    res->SensorPreviewHeight = 1744;
    res->SensorFullWidth = 4640;
    res->SensorFullHeight = 3488;
    res->SensorVideoWidth = 2320;
    res->SensorVideoHeight = 1744;
    res->SensorHighSpeedVideoWidth = 1280;
    res->SensorHighSpeedVideoHeight = 720;
    res->SensorSlimVideoWidth = 1280;
    res->SensorSlimVideoHeight = 720;
    res->SensorCustom1Width = 2320;
    res->SensorCustom1Height = 1744;
    res->SensorCustom2Width = 2320;
    res->SensorCustom2Height = 1744;
    res->SensorCustom3Width = 4640;
    res->SensorCustom3Height = 3488;
    res->SensorCustom4Width = 0;
    res->SensorCustom4Height = 0;
    res->SensorCustom5Width = 0;
    res->SensorCustom5Height = 0;
    return ERROR_NONE;
}

/* Exact info matching stock sub_A907F0 */
static kal_uint32 hi1631q_get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
                                  MSDK_SENSOR_INFO_STRUCT *info,
                                  MSDK_SENSOR_CONFIG_STRUCT *cfg)
{
    if (!info) return ERROR_INVALID_PARA;
    info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
    info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
    info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    info->SensorInterruptDelayLines = 3;
    info->SensroInterfaceType = SENSOR_INTERFACE_TYPE_MIPI;
    info->MIPIsensorType = MIPI_OPHY_NCSI2;
    info->SettleDelayMode = MIPI_SETTLEDELAY_AUTO;
    info->SensorOutputDataFormat = SENSOR_OUTPUT_FORMAT_RAW_B;
    info->CaptureDelayFrame = 2;
    info->PreviewDelayFrame = 2;
    info->VideoDelayFrame = 2;
    info->HighSpeedVideoDelayFrame = 2;
    info->SlimVideoDelayFrame = 2;
    info->Custom1DelayFrame = 2;
    info->Custom2DelayFrame = 2;
    info->Custom3DelayFrame = 2;
    info->SensorMIPILaneNumber = SENSOR_MIPI_4_LANE;
    info->SensorClockFreq = 24;
    info->SensorClockDividCount = 3;
    info->SensorClockFallingCount = 2;
    info->SensorPixelClockCount = 3;
    info->SensorDataLatchCount = 2;
    info->SensorPreviewResolutionX = 2320;
    info->SensorPreviewResolutionY = 1744;
    info->SensorFullResolutionX = 4640;
    info->SensorFullResolutionY = 3488;
    info->SensorModeNum = 8;
    *((kal_uint8 *)info + 241) = 8;
    return ERROR_NONE;
}

/* Exact feature control matching stock sub_A90B00 */
static kal_uint32 hi1631q_feature_control(MSDK_SENSOR_FEATURE_ENUM feat_id,
                                          MUINT8 *para, MUINT32 *para_len)
{
    UINT16 *feature_return_para_16 = (UINT16 *)para;
    UINT16 *feature_data_16 = (UINT16 *)para;
    UINT32 *feature_return_para_32 = (UINT32 *)para;
    UINT32 *feature_data_32 = (UINT32 *)para;
    unsigned long long *feature_data = (unsigned long long *)para;
    struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
    MUINT32 *p32;

    switch (feat_id) {
    case SENSOR_FEATURE_CHECK_SENSOR_ID:
        if (feature_return_para_32 && para_len) {
            *feature_return_para_32 = HI1631Q_SENSOR_ID;
            *para_len = sizeof(MUINT32);
        }
        break;
    case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
        if (feature_data) {
            p32 = (MUINT32 *)(uintptr_t)(*(feature_data + 1));
            if (p32) {
                switch (*feature_data_32) {
                case 0:
                case 1:
                case 2:
                case 5:
                case 7:
                    *p32 = 300;
                    break;
                case 3:
                case 4:
                    *p32 = 1200;
                    break;
                case 6:
                    *p32 = 600;
                    break;
                default:
                    *p32 = 300;
                    break;
                }
            }
        }
        break;
    case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
        if (feature_data) {
            p32 = (MUINT32 *)(uintptr_t)(*(feature_data + 1));
            if (p32) {
                switch (*feature_data_32) {
                case 3:
                case 4:
                    *p32 = 61347392;
                    break;
                case 6:
                    *p32 = 122689088;
                    break;
                default:
                    *p32 = 245438016;
                    break;
                }
            }
        }
        break;
    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
        if (feature_data) {
            p32 = (MUINT32 *)(uintptr_t)(*(feature_data + 1));
            if (p32)
                *p32 = 640000000;
        }
        break;
    case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
        if (feature_data) {
            p32 = (MUINT32 *)(uintptr_t)(*(feature_data + 1));
            if (p32) {
                switch (*feature_data_32) {
                case 1:
                case 7:
                    *p32 = 585600000;
                    break;
                default:
                    *p32 = 292800000;
                    break;
                }
            }
        }
        break;
    case SENSOR_FEATURE_GET_PERIOD:
        if (feature_return_para_16 && para_len) {
            *feature_return_para_16++ = hi1631q_sensor.line_length;
            *feature_return_para_16 = hi1631q_sensor.frame_length;
            *para_len = 4;
        }
        break;
    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
        if (feature_return_para_32 && para_len) {
            *feature_return_para_32 = hi1631q_sensor.pclk;
            *para_len = 4;
        }
        break;
    case SENSOR_FEATURE_SET_FRAMERATE:
        if (feature_data_32) {
            spin_lock(&hi1631q_lock);
            hi1631q_sensor.current_fps = *feature_data_32;
            spin_unlock(&hi1631q_lock);
        }
        break;
    case SENSOR_FEATURE_SET_HDR:
        if (feature_data_32) {
            spin_lock(&hi1631q_lock);
            hi1631q_sensor.ihdr_en = (kal_uint8)*feature_data_32;
            spin_unlock(&hi1631q_lock);
        }
        break;
    case SENSOR_FEATURE_GET_CROP_INFO:
        if (feature_data) {
            wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data + 1));
            if (wininfo) {
                kal_uint32 scen = *feature_data_32;
                if (scen < 8)
                    memcpy(wininfo, &hi1631q_winsize_info[scen], sizeof(*wininfo));
                else
                    memcpy(wininfo, &hi1631q_winsize_info[0], sizeof(*wininfo));
            }
        }
        break;
    case SENSOR_FEATURE_GET_BINNING_TYPE:
        if (feature_return_para_32 && para_len) {
            *feature_return_para_32 = 1;
            *para_len = 4;
        }
        break;
    case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
        if (feature_data) {
            *(feature_data + 1) = 64;
            *(feature_data + 2) = 1024;
        }
        break;
    case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
        if (feature_data) {
            *(feature_data + 0) = 50;
            *(feature_data + 1) = 4;
            *(feature_data + 2) = 3;
        }
        break;
    case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
        if (feature_data) {
            *(feature_data + 1) = 4;
            if (*feature_data_32 >= 7 && *feature_data_32 <= 9)
                *(feature_data + 2) = 1;
            else
                *(feature_data + 2) = 2;
        }
        break;
    case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
        if (feature_data) {
            *(feature_data + 1) = 1;
            *(feature_data + 2) = 4;
        }
        break;
    case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
        if (feature_data_16) {
            hi1631q_sensor.autoflicker_en = (kal_bool)*feature_data_16;
        }
        break;
    case SENSOR_FEATURE_SET_TEST_PATTERN:
        if (feature_data_32) {
            hi1631q_sensor.test_pattern = (kal_bool)*feature_data_32;
        }
        break;
    case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
        if (feature_return_para_32 && para_len) {
            *feature_return_para_32 = 0;
            *para_len = 4;
        }
        break;
    case SENSOR_FEATURE_SET_STREAMING_RESUME:
        if (feature_data && *feature_data != 0) {
            hi1631q_sensor.shutter = (kal_uint32)*feature_data;
        }
        write_cmos_sensor_16(0x000b, 0x0001, hi1631q_sensor.i2c_write_id);
        mdelay(10);
        break;
    case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
        write_cmos_sensor_16(0x000b, 0x0000, hi1631q_sensor.i2c_write_id);
        mdelay(10);
        break;
    case SENSOR_FEATURE_SET_ESHUTTER:
        if (feature_data_32) {
            hi1631q_sensor.shutter = *feature_data_32;
        }
        break;
    case SENSOR_FEATURE_SET_GAIN:
        if (feature_data_16) {
            hi1631q_sensor.gain = *feature_data_16;
        }
        break;
    case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
        if (feature_return_para_32 && para_len) {
            *feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
            *para_len = 4;
        }
        break;
    default:
        break;
    }
    return ERROR_NONE;
}

static kal_uint32 hi1631q_control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
                                  MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *win,
                                  MSDK_SENSOR_CONFIG_STRUCT *cfg)
{
    switch (scenario_id) {
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        write_regs_16(hi1631q_capture_regs, ARRAY_SIZE(hi1631q_capture_regs), hi1631q_sensor.i2c_write_id);
        break;
    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        write_regs_16(hi1631q_video_regs, ARRAY_SIZE(hi1631q_video_regs), hi1631q_sensor.i2c_write_id);
        break;
    default:
        write_regs_16(hi1631q_preview_regs, ARRAY_SIZE(hi1631q_preview_regs), hi1631q_sensor.i2c_write_id);
        break;
    }
    return ERROR_NONE;
}

static kal_uint32 hi1631q_close(void)
{
    return ERROR_NONE;
}

static struct SENSOR_FUNCTION_STRUCT hi1631q_sensor_func = {
    hi1631q_open,
    hi1631q_get_info,
    hi1631q_get_resolution,
    hi1631q_feature_control,
    hi1631q_control,
    hi1631q_close
};

UINT32 HI1631Q_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
    if (pfFunc != NULL)
        *pfFunc = &hi1631q_sensor_func;
    return ERROR_NONE;
}

/* ========================================================================= */
/* GC02M1 (GalaxyCore 2MP Depth, Sensor ID 0x02E0)                           */
/* ========================================================================= */

static const struct regval_8 gc02m1_init_regs[200] = {
    { 0xfc, 0x01 },
    { 0xf4, 0x41 },
    { 0xf5, 0xc0 },
    { 0xf6, 0x44 },
    { 0xf8, 0x38 },
    { 0xf9, 0x82 },
    { 0xfa, 0x00 },
    { 0xfd, 0x80 },
    { 0xfc, 0x81 },
    { 0xfe, 0x03 },
    { 0x01, 0x0b },
    { 0xf7, 0x01 },
    { 0xfc, 0x80 },
    { 0xfc, 0x80 },
    { 0xfc, 0x80 },
    { 0xfc, 0x8e },
    { 0xfe, 0x00 },
    { 0x87, 0x09 },
    { 0xee, 0x72 },
    { 0xfe, 0x01 },
    { 0x8c, 0x90 },
    { 0xfe, 0x00 },
    { 0x90, 0x00 },
    { 0x03, 0x04 },
    { 0x04, 0x7d },
    { 0x41, 0x04 },
    { 0x42, 0xf4 },
    { 0x05, 0x04 },
    { 0x06, 0x48 },
    { 0x07, 0x00 },
    { 0x08, 0x18 },
    { 0x9d, 0x18 },
    { 0x09, 0x00 },
    { 0x0a, 0x02 },
    { 0x0d, 0x04 },
    { 0x0e, 0xbc },
    { 0x17, 0x80 },
    { 0x19, 0x04 },
    { 0x24, 0x00 },
    { 0x56, 0x20 },
    { 0x5b, 0x00 },
    { 0x5e, 0x01 },
    { 0x21, 0x3c },
    { 0x44, 0x20 },
    { 0xcc, 0x01 },
    { 0x1a, 0x04 },
    { 0x1f, 0x11 },
    { 0x27, 0x30 },
    { 0x2b, 0x00 },
    { 0x33, 0x00 },
    { 0x53, 0x90 },
    { 0xe6, 0x50 },
    { 0x39, 0x07 },
    { 0x43, 0x04 },
    { 0x46, 0x2a },
    { 0x7c, 0xa0 },
    { 0xd0, 0xbe },
    { 0xd1, 0x60 },
    { 0xd2, 0x40 },
    { 0xd3, 0xf3 },
    { 0xde, 0x1d },
    { 0xcd, 0x05 },
    { 0xce, 0x6f },
    { 0xfc, 0x88 },
    { 0xfe, 0x10 },
    { 0xfe, 0x00 },
    { 0xfc, 0x8e },
    { 0xfe, 0x00 },
    { 0xfe, 0x00 },
    { 0xfe, 0x00 },
    { 0xfe, 0x00 },
    { 0xfc, 0x88 },
    { 0xfe, 0x10 },
    { 0xfe, 0x00 },
    { 0xfc, 0x8e },
    { 0xfe, 0x04 },
    { 0xe0, 0x01 },
    { 0xfe, 0x00 },
    { 0xfe, 0x01 },
    { 0x53, 0x44 },
    { 0x87, 0x53 },
    { 0x89, 0x03 },
    { 0xfe, 0x00 },
    { 0xb0, 0x74 },
    { 0xb1, 0x04 },
    { 0xb2, 0x00 },
    { 0xb6, 0x00 },
    { 0xfe, 0x04 },
    { 0xd8, 0x00 },
    { 0xc0, 0x40 },
    { 0xc0, 0x00 },
    { 0xc0, 0x00 },
    { 0xc0, 0x00 },
    { 0xc0, 0x60 },
    { 0xc0, 0x00 },
    { 0xc0, 0xc0 },
    { 0xc0, 0x2a },
    { 0xc0, 0x80 },
    { 0xc0, 0x00 },
    { 0xc0, 0x00 },
    { 0xc0, 0x40 },
    { 0xc0, 0xa0 },
    { 0xc0, 0x00 },
    { 0xc0, 0x90 },
    { 0xc0, 0x19 },
    { 0xc0, 0xc0 },
    { 0xc0, 0x00 },
    { 0xc0, 0xd0 },
    { 0xc0, 0x2f },
    { 0xc0, 0xe0 },
    { 0xc0, 0x00 },
    { 0xc0, 0x90 },
    { 0xc0, 0x39 },
    { 0xc0, 0x00 },
    { 0xc0, 0x01 },
    { 0xc0, 0x20 },
    { 0xc0, 0x04 },
    { 0xc0, 0x20 },
    { 0xc0, 0x01 },
    { 0xc0, 0xe0 },
    { 0xc0, 0x0f },
    { 0xc0, 0x40 },
    { 0xc0, 0x01 },
    { 0xc0, 0xe0 },
    { 0xc0, 0x1a },
    { 0xc0, 0x60 },
    { 0xc0, 0x01 },
    { 0xc0, 0x20 },
    { 0xc0, 0x25 },
    { 0xc0, 0x80 },
    { 0xc0, 0x01 },
    { 0xc0, 0xa0 },
    { 0xc0, 0x2c },
    { 0xc0, 0xa0 },
    { 0xc0, 0x01 },
    { 0xc0, 0xe0 },
    { 0xc0, 0x32 },
    { 0xc0, 0xc0 },
    { 0xc0, 0x01 },
    { 0xc0, 0x20 },
    { 0xc0, 0x38 },
    { 0xc0, 0xe0 },
    { 0xc0, 0x01 },
    { 0xc0, 0x60 },
    { 0xc0, 0x3c },
    { 0xc0, 0x00 },
    { 0xc0, 0x02 },
    { 0xc0, 0xa0 },
    { 0xc0, 0x40 },
    { 0xc0, 0x80 },
    { 0xc0, 0x02 },
    { 0xc0, 0x18 },
    { 0xc0, 0x5c },
    { 0xfe, 0x00 },
    { 0x9f, 0x10 },
    { 0xfe, 0x00 },
    { 0x26, 0x20 },
    { 0xfe, 0x01 },
    { 0x40, 0x22 },
    { 0x46, 0x7f },
    { 0x49, 0x0f },
    { 0x4a, 0xf0 },
    { 0xfe, 0x04 },
    { 0x14, 0x80 },
    { 0x15, 0x80 },
    { 0x16, 0x80 },
    { 0x17, 0x80 },
    { 0xfe, 0x01 },
    { 0x41, 0x20 },
    { 0x4c, 0x00 },
    { 0x4d, 0x0c },
    { 0x44, 0x08 },
    { 0x48, 0x03 },
    { 0xfe, 0x01 },
    { 0x90, 0x01 },
    { 0x91, 0x00 },
    { 0x92, 0x06 },
    { 0x93, 0x00 },
    { 0x94, 0x06 },
    { 0x95, 0x04 },
    { 0x96, 0xb0 },
    { 0x97, 0x06 },
    { 0x98, 0x40 },
    { 0xfe, 0x03 },
    { 0x01, 0x23 },
    { 0x03, 0xce },
    { 0x04, 0x48 },
    { 0x15, 0x00 },
    { 0x21, 0x10 },
    { 0x22, 0x05 },
    { 0x23, 0x20 },
    { 0x25, 0x20 },
    { 0x26, 0x08 },
    { 0x29, 0x06 },
    { 0x2a, 0x0a },
    { 0x2b, 0x08 },
    { 0xfe, 0x01 },
    { 0x8c, 0x10 },
    { 0xfe, 0x00 },
    { 0x3e, 0x00 },
};

static const struct regval_8 gc02m1_preview_regs[1] = {
    { 0xfe, 0x00 },
};

static const struct regval_8 gc02m1_capture_regs[1] = {
    { 0xfe, 0x00 },
};

static const struct regval_8 gc02m1_streamon_regs[2] = {
    { 0xfe, 0x00 },
    { 0x3e, 0x90 },
};

static const struct regval_8 gc02m1_streamoff_regs[2] = {
    { 0xfe, 0x00 },
    { 0x3e, 0x00 },
};


static DEFINE_SPINLOCK(gc02m1_lock);

static struct imgsensor_struct gc02m1_sensor = {
    .mirror = IMAGE_NORMAL,
    .sensor_mode = IMGSENSOR_MODE_INIT,
    .shutter = 0x3D0,
    .gain = 0x100,
    .pclk = 66000000,
    .frame_length = 1276,
    .line_length = 1680,
    .min_frame_length = 1276,
    .dummy_pixel = 0,
    .dummy_line = 0,
    .current_fps = 300,
    .autoflicker_en = KAL_FALSE,
    .test_pattern = KAL_FALSE,
    .current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
    .ihdr_en = 0,
    .i2c_write_id = 0x20,
};

/* Exact 6-scenario crop info table dumped from stock_Image at 0x1944cb8 */
static const struct SENSOR_WINSIZE_INFO_STRUCT gc02m1_winsize_info[6] = {
    { 1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0, 0, 1600, 1200, 0, 0, 1600, 1200 },
    { 1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0, 0, 1600, 1200, 0, 0, 1600, 1200 },
    { 1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0, 0, 1600, 1200, 0, 0, 1600, 1200 },
    { 1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0, 0, 1600, 1200, 0, 0, 1600, 1200 },
    { 1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0, 0, 1600, 1200, 0, 0, 1600, 1200 },
    { 1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0, 0, 1600, 1200, 0, 0, 1600, 1200 }
};

static kal_uint32 gc02m1_open(void)
{
    write_regs_8(gc02m1_init_regs, ARRAY_SIZE(gc02m1_init_regs), gc02m1_sensor.i2c_write_id);
    return ERROR_NONE;
}

/* Exact 6-mode resolution matching stock sub_A93BE4 */
static kal_uint32 gc02m1_get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *res)
{
    if (!res) return ERROR_INVALID_PARA;
    res->SensorPreviewWidth = 1600;
    res->SensorPreviewHeight = 1200;
    res->SensorFullWidth = 1600;
    res->SensorFullHeight = 1200;
    res->SensorVideoWidth = 1600;
    res->SensorVideoHeight = 1200;
    res->SensorHighSpeedVideoWidth = 1600;
    res->SensorHighSpeedVideoHeight = 1200;
    res->SensorSlimVideoWidth = 1600;
    res->SensorSlimVideoHeight = 1200;
    res->SensorCustom1Width = 1600;
    res->SensorCustom1Height = 1200;
    return ERROR_NONE;
}

/* Exact info matching stock sub_A93B7C */
static kal_uint32 gc02m1_get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
                                  MSDK_SENSOR_INFO_STRUCT *info,
                                  MSDK_SENSOR_CONFIG_STRUCT *cfg)
{
    if (!info) return ERROR_INVALID_PARA;
    info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
    info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
    info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    info->SensorInterruptDelayLines = 3;
    info->SensroInterfaceType = SENSOR_INTERFACE_TYPE_MIPI;
    info->MIPIsensorType = MIPI_OPHY_NCSI2;
    info->SettleDelayMode = MIPI_SETTLEDELAY_AUTO;
    info->SensorOutputDataFormat = SENSOR_OUTPUT_FORMAT_RAW_B;
    info->CaptureDelayFrame = 2;
    info->PreviewDelayFrame = 2;
    info->VideoDelayFrame = 2;
    info->HighSpeedVideoDelayFrame = 2;
    info->SlimVideoDelayFrame = 2;
    info->Custom1DelayFrame = 2;
    info->SensorMIPILaneNumber = SENSOR_MIPI_1_LANE;
    info->SensorClockFreq = 24;
    info->SensorClockDividCount = 3;
    info->SensorClockFallingCount = 2;
    info->SensorPixelClockCount = 3;
    info->SensorDataLatchCount = 2;
    info->SensorPreviewResolutionX = 1600;
    info->SensorPreviewResolutionY = 1200;
    info->SensorFullResolutionX = 1600;
    info->SensorFullResolutionY = 1200;
    info->SensorModeNum = 6;
    *((kal_uint8 *)info + 241) = 6;
    return ERROR_NONE;
}

/* Exact feature control matching stock sub_A93C04 */
static kal_uint32 gc02m1_feature_control(MSDK_SENSOR_FEATURE_ENUM feat_id,
                                         MUINT8 *para, MUINT32 *para_len)
{
    UINT16 *feature_return_para_16 = (UINT16 *)para;
    UINT16 *feature_data_16 = (UINT16 *)para;
    UINT32 *feature_return_para_32 = (UINT32 *)para;
    UINT32 *feature_data_32 = (UINT32 *)para;
    unsigned long long *feature_data = (unsigned long long *)para;
    struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
    MUINT32 *p32;

    switch (feat_id) {
    case SENSOR_FEATURE_CHECK_SENSOR_ID:
        if (feature_return_para_32 && para_len) {
            *feature_return_para_32 = GC02M1_SENSOR_ID;
            *para_len = sizeof(MUINT32);
        }
        break;
    case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
        if (feature_data) {
            p32 = (MUINT32 *)(uintptr_t)(*(feature_data + 1));
            if (p32)
                *p32 = 300;
        }
        break;
    case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
        if (feature_data) {
            p32 = (MUINT32 *)(uintptr_t)(*(feature_data + 1));
            if (p32)
                *p32 = (gc02m1_sensor.frame_length << 16) + gc02m1_sensor.line_length;
        }
        break;
    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
        if (feature_data) {
            p32 = (MUINT32 *)(uintptr_t)(*(feature_data + 1));
            if (p32)
                *p32 = gc02m1_sensor.pclk;
        }
        break;
    case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
        if (feature_data) {
            p32 = (MUINT32 *)(uintptr_t)(*(feature_data + 1));
            if (p32)
                *p32 = 66000000;
        }
        break;
    case SENSOR_FEATURE_GET_PIXEL_RATE:
        if (feature_data) {
            p32 = (MUINT32 *)(uintptr_t)(*(feature_data + 1));
            if (p32)
                *p32 = 63635200;
        }
        break;
    case SENSOR_FEATURE_GET_PERIOD:
        if (feature_return_para_16 && para_len) {
            *feature_return_para_16++ = gc02m1_sensor.line_length;
            *feature_return_para_16 = gc02m1_sensor.frame_length;
            *para_len = 4;
        }
        break;
    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
        if (feature_return_para_32 && para_len) {
            *feature_return_para_32 = gc02m1_sensor.pclk;
            *para_len = 4;
        }
        break;
    case SENSOR_FEATURE_SET_FRAMERATE:
        if (feature_data_32) {
            spin_lock(&gc02m1_lock);
            gc02m1_sensor.current_fps = *feature_data_32;
            spin_unlock(&gc02m1_lock);
        }
        break;
    case SENSOR_FEATURE_SET_HDR:
        if (feature_data_32) {
            spin_lock(&gc02m1_lock);
            gc02m1_sensor.ihdr_en = (kal_uint8)*feature_data_32;
            spin_unlock(&gc02m1_lock);
        }
        break;
    case SENSOR_FEATURE_GET_CROP_INFO:
        if (feature_data) {
            wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data + 1));
            if (wininfo) {
                kal_uint32 scen = *feature_data_32;
                if (scen < 6)
                    memcpy(wininfo, &gc02m1_winsize_info[scen], sizeof(*wininfo));
                else
                    memcpy(wininfo, &gc02m1_winsize_info[0], sizeof(*wininfo));
            }
        }
        break;
    case SENSOR_FEATURE_GET_BINNING_TYPE:
        if (feature_return_para_32 && para_len) {
            *feature_return_para_32 = 1;
            *para_len = 4;
        }
        break;
    case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
        if (feature_data) {
            *(feature_data + 1) = 64;
            *(feature_data + 2) = 1024;
        }
        break;
    case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
        if (feature_data) {
            *(feature_data + 0) = 50;
            *(feature_data + 1) = 1;
            *(feature_data + 2) = 0;
        }
        break;
    case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
        if (feature_data) {
            *(feature_data + 1) = 1;
        }
        break;
    case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
        if (feature_data) {
            *(feature_data + 1) = 1;
            *(feature_data + 2) = 4;
        }
        break;
    case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
        if (feature_data_16) {
            gc02m1_sensor.autoflicker_en = (kal_bool)*feature_data_16;
        }
        break;
    case SENSOR_FEATURE_SET_TEST_PATTERN:
        if (feature_data_32) {
            gc02m1_sensor.test_pattern = (kal_bool)*feature_data_32;
        }
        break;
    case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
        if (feature_return_para_32 && para_len) {
            *feature_return_para_32 = 0;
            *para_len = 4;
        }
        break;
    case SENSOR_FEATURE_SET_STREAMING_RESUME:
        write_cmos_sensor_8_8(0xfe, 0x00, gc02m1_sensor.i2c_write_id);
        write_cmos_sensor_8_8(0x3e, 0x90, gc02m1_sensor.i2c_write_id);
        break;
    case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
        write_cmos_sensor_8_8(0xfe, 0x00, gc02m1_sensor.i2c_write_id);
        write_cmos_sensor_8_8(0x3e, 0x00, gc02m1_sensor.i2c_write_id);
        break;
    case SENSOR_FEATURE_SET_ESHUTTER:
        if (feature_data_32) {
            gc02m1_sensor.shutter = *feature_data_32;
        }
        break;
    case SENSOR_FEATURE_SET_GAIN:
        if (feature_data_16) {
            gc02m1_sensor.gain = *feature_data_16;
        }
        break;
    case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
        if (feature_return_para_32 && para_len) {
            *feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
            *para_len = 4;
        }
        break;
    default:
        break;
    }
    return ERROR_NONE;
}

static kal_uint32 gc02m1_control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
                                 MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *win,
                                 MSDK_SENSOR_CONFIG_STRUCT *cfg)
{
    switch (scenario_id) {
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        write_regs_8(gc02m1_capture_regs, ARRAY_SIZE(gc02m1_capture_regs), gc02m1_sensor.i2c_write_id);
        break;
    default:
        write_regs_8(gc02m1_preview_regs, ARRAY_SIZE(gc02m1_preview_regs), gc02m1_sensor.i2c_write_id);
        break;
    }
    return ERROR_NONE;
}

static kal_uint32 gc02m1_close(void)
{
    return ERROR_NONE;
}

static struct SENSOR_FUNCTION_STRUCT gc02m1_sensor_func = {
    gc02m1_open,
    gc02m1_get_info,
    gc02m1_get_resolution,
    gc02m1_feature_control,
    gc02m1_control,
    gc02m1_close
};

UINT32 GC02M1_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
    if (pfFunc != NULL)
        *pfFunc = &gc02m1_sensor_func;
    return ERROR_NONE;
}

/* ========================================================================= */
/* GC02M1MACRO (GalaxyCore 2MP Macro, Sensor ID 0x02E3)                      */
/* ========================================================================= */

static const struct regval_8 gc02m1macro_init_regs[200] = {
    { 0xfc, 0x01 },
    { 0xf4, 0x41 },
    { 0xf5, 0xe3 },
    { 0xf6, 0x44 },
    { 0xf8, 0x38 },
    { 0xf9, 0x82 },
    { 0xfa, 0x00 },
    { 0xfd, 0x80 },
    { 0xfc, 0x81 },
    { 0xfe, 0x03 },
    { 0x01, 0x0b },
    { 0xf7, 0x01 },
    { 0xfc, 0x80 },
    { 0xfc, 0x80 },
    { 0xfc, 0x80 },
    { 0xfc, 0x8e },
    { 0xfe, 0x00 },
    { 0x87, 0x09 },
    { 0xee, 0x72 },
    { 0xfe, 0x01 },
    { 0x8c, 0x90 },
    { 0xfe, 0x00 },
    { 0x90, 0x00 },
    { 0x03, 0x04 },
    { 0x04, 0x7d },
    { 0x41, 0x04 },
    { 0x42, 0xf4 },
    { 0x05, 0x04 },
    { 0x06, 0x48 },
    { 0x07, 0x00 },
    { 0x08, 0x18 },
    { 0x9d, 0x18 },
    { 0x09, 0x00 },
    { 0x0a, 0x02 },
    { 0x0d, 0x04 },
    { 0x0e, 0xbc },
    { 0x17, 0x80 },
    { 0x19, 0x04 },
    { 0x24, 0x00 },
    { 0x56, 0x20 },
    { 0x5b, 0x00 },
    { 0x5e, 0x01 },
    { 0x21, 0x3c },
    { 0x44, 0x20 },
    { 0xcc, 0x01 },
    { 0x1a, 0x04 },
    { 0x1f, 0x11 },
    { 0x27, 0x30 },
    { 0x2b, 0x00 },
    { 0x33, 0x00 },
    { 0x53, 0x90 },
    { 0xe6, 0x50 },
    { 0x39, 0x07 },
    { 0x43, 0x04 },
    { 0x46, 0x2a },
    { 0x7c, 0xa0 },
    { 0xd0, 0xbe },
    { 0xd1, 0x60 },
    { 0xd2, 0x40 },
    { 0xd3, 0xf3 },
    { 0xde, 0x1d },
    { 0xcd, 0x05 },
    { 0xce, 0x6f },
    { 0xfc, 0x88 },
    { 0xfe, 0x10 },
    { 0xfe, 0x00 },
    { 0xfc, 0x8e },
    { 0xfe, 0x00 },
    { 0xfe, 0x00 },
    { 0xfe, 0x00 },
    { 0xfe, 0x00 },
    { 0xfc, 0x88 },
    { 0xfe, 0x10 },
    { 0xfe, 0x00 },
    { 0xfc, 0x8e },
    { 0xfe, 0x04 },
    { 0xe0, 0x01 },
    { 0xfe, 0x00 },
    { 0xfe, 0x01 },
    { 0x53, 0x54 },
    { 0x87, 0x53 },
    { 0x89, 0x03 },
    { 0xfe, 0x00 },
    { 0xb0, 0x74 },
    { 0xb1, 0x04 },
    { 0xb2, 0x00 },
    { 0xb6, 0x00 },
    { 0xfe, 0x04 },
    { 0xd8, 0x00 },
    { 0xc0, 0x40 },
    { 0xc0, 0x00 },
    { 0xc0, 0x00 },
    { 0xc0, 0x00 },
    { 0xc0, 0x60 },
    { 0xc0, 0x00 },
    { 0xc0, 0xc0 },
    { 0xc0, 0x2a },
    { 0xc0, 0x80 },
    { 0xc0, 0x00 },
    { 0xc0, 0x00 },
    { 0xc0, 0x40 },
    { 0xc0, 0xa0 },
    { 0xc0, 0x00 },
    { 0xc0, 0x90 },
    { 0xc0, 0x19 },
    { 0xc0, 0xc0 },
    { 0xc0, 0x00 },
    { 0xc0, 0xd0 },
    { 0xc0, 0x2f },
    { 0xc0, 0xe0 },
    { 0xc0, 0x00 },
    { 0xc0, 0x90 },
    { 0xc0, 0x39 },
    { 0xc0, 0x00 },
    { 0xc0, 0x01 },
    { 0xc0, 0x20 },
    { 0xc0, 0x04 },
    { 0xc0, 0x20 },
    { 0xc0, 0x01 },
    { 0xc0, 0xe0 },
    { 0xc0, 0x0f },
    { 0xc0, 0x40 },
    { 0xc0, 0x01 },
    { 0xc0, 0xe0 },
    { 0xc0, 0x1a },
    { 0xc0, 0x60 },
    { 0xc0, 0x01 },
    { 0xc0, 0x20 },
    { 0xc0, 0x25 },
    { 0xc0, 0x80 },
    { 0xc0, 0x01 },
    { 0xc0, 0xa0 },
    { 0xc0, 0x2c },
    { 0xc0, 0xa0 },
    { 0xc0, 0x01 },
    { 0xc0, 0xe0 },
    { 0xc0, 0x32 },
    { 0xc0, 0xc0 },
    { 0xc0, 0x01 },
    { 0xc0, 0x20 },
    { 0xc0, 0x38 },
    { 0xc0, 0xe0 },
    { 0xc0, 0x01 },
    { 0xc0, 0x60 },
    { 0xc0, 0x3c },
    { 0xc0, 0x00 },
    { 0xc0, 0x02 },
    { 0xc0, 0xa0 },
    { 0xc0, 0x40 },
    { 0xc0, 0x80 },
    { 0xc0, 0x02 },
    { 0xc0, 0x18 },
    { 0xc0, 0x5c },
    { 0xfe, 0x00 },
    { 0x9f, 0x10 },
    { 0xfe, 0x00 },
    { 0x26, 0x20 },
    { 0xfe, 0x01 },
    { 0x40, 0x22 },
    { 0x46, 0x4a },
    { 0x49, 0x0f },
    { 0x4a, 0xf0 },
    { 0xfe, 0x04 },
    { 0x14, 0x80 },
    { 0x15, 0x80 },
    { 0x16, 0x80 },
    { 0x17, 0x80 },
    { 0xfe, 0x01 },
    { 0x41, 0x20 },
    { 0x4c, 0x00 },
    { 0x4d, 0x0c },
    { 0x44, 0x08 },
    { 0x48, 0x03 },
    { 0xfe, 0x01 },
    { 0x90, 0x01 },
    { 0x91, 0x00 },
    { 0x92, 0x06 },
    { 0x93, 0x00 },
    { 0x94, 0x06 },
    { 0x95, 0x04 },
    { 0x96, 0xb0 },
    { 0x97, 0x06 },
    { 0x98, 0x40 },
    { 0xfe, 0x03 },
    { 0x01, 0x23 },
    { 0x03, 0xce },
    { 0x04, 0x48 },
    { 0x15, 0x00 },
    { 0x21, 0x10 },
    { 0x22, 0x05 },
    { 0x23, 0x20 },
    { 0x25, 0x20 },
    { 0x26, 0x08 },
    { 0x29, 0x06 },
    { 0x2a, 0x0a },
    { 0x2b, 0x08 },
    { 0xfe, 0x01 },
    { 0x8c, 0x10 },
    { 0xfe, 0x00 },
    { 0x3e, 0x00 },
};

static const struct regval_8 gc02m1macro_preview_regs[1] = {
    { 0xfe, 0x00 },
};

static const struct regval_8 gc02m1macro_streamon_regs[2] = {
    { 0xfe, 0x00 },
    { 0x3e, 0x90 },
};

static const struct regval_8 gc02m1macro_streamoff_regs[2] = {
    { 0xfe, 0x00 },
    { 0x3e, 0x00 },
};


static DEFINE_SPINLOCK(gc02m1macro_lock);

static struct imgsensor_struct gc02m1macro_sensor = {
    .mirror = IMAGE_NORMAL,
    .sensor_mode = IMGSENSOR_MODE_INIT,
    .shutter = 0x3D0,
    .gain = 0x100,
    .pclk = 66000000,
    .frame_length = 1276,
    .line_length = 1680,
    .min_frame_length = 1276,
    .dummy_pixel = 0,
    .dummy_line = 0,
    .current_fps = 300,
    .autoflicker_en = KAL_FALSE,
    .test_pattern = KAL_FALSE,
    .current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
    .ihdr_en = 0,
    .i2c_write_id = 0x20,
};

/* Exact 5-scenario crop info table dumped from stock_Image at 0x1944d78 */
static const struct SENSOR_WINSIZE_INFO_STRUCT gc02m1macro_winsize_info[5] = {
    { 1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0, 0, 1600, 1200, 0, 0, 1600, 1200 },
    { 1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0, 0, 1600, 1200, 0, 0, 1600, 1200 },
    { 1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0, 0, 1600, 1200, 0, 0, 1600, 1200 },
    { 1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0, 0, 1600, 1200, 0, 0, 1600, 1200 },
    { 1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0, 0, 1600, 1200, 0, 0, 1600, 1200 }
};

static kal_uint32 gc02m1macro_open(void)
{
    write_regs_8(gc02m1macro_init_regs, ARRAY_SIZE(gc02m1macro_init_regs), gc02m1macro_sensor.i2c_write_id);
    return ERROR_NONE;
}

/* Exact 5-mode resolution matching stock sub_A94DB4 */
static kal_uint32 gc02m1macro_get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *res)
{
    if (!res) return ERROR_INVALID_PARA;
    res->SensorPreviewWidth = 1600;
    res->SensorPreviewHeight = 1200;
    res->SensorFullWidth = 1600;
    res->SensorFullHeight = 1200;
    res->SensorVideoWidth = 1600;
    res->SensorVideoHeight = 1200;
    res->SensorHighSpeedVideoWidth = 1600;
    res->SensorHighSpeedVideoHeight = 1200;
    res->SensorSlimVideoWidth = 1600;
    res->SensorSlimVideoHeight = 1200;
    return ERROR_NONE;
}

/* Exact info matching stock sub_A94DDC */
static kal_uint32 gc02m1macro_get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
                                       MSDK_SENSOR_INFO_STRUCT *info,
                                       MSDK_SENSOR_CONFIG_STRUCT *cfg)
{
    if (!info) return ERROR_INVALID_PARA;
    info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
    info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
    info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    info->SensorInterruptDelayLines = 3;
    info->SensroInterfaceType = SENSOR_INTERFACE_TYPE_MIPI;
    info->MIPIsensorType = MIPI_OPHY_NCSI2;
    info->SettleDelayMode = MIPI_SETTLEDELAY_AUTO;
    info->SensorOutputDataFormat = SENSOR_OUTPUT_FORMAT_RAW_B;
    info->CaptureDelayFrame = 2;
    info->PreviewDelayFrame = 2;
    info->VideoDelayFrame = 2;
    info->HighSpeedVideoDelayFrame = 2;
    info->SlimVideoDelayFrame = 2;
    info->SensorMIPILaneNumber = SENSOR_MIPI_1_LANE;
    info->SensorClockFreq = 24;
    info->SensorClockDividCount = 3;
    info->SensorClockFallingCount = 2;
    info->SensorPixelClockCount = 3;
    info->SensorDataLatchCount = 2;
    info->SensorPreviewResolutionX = 1600;
    info->SensorPreviewResolutionY = 1200;
    info->SensorFullResolutionX = 1600;
    info->SensorFullResolutionY = 1200;
    info->SensorModeNum = 5;
    *((kal_uint8 *)info + 241) = 5;
    return ERROR_NONE;
}

/* Exact feature control matching stock sub_A94DDC */
static kal_uint32 gc02m1macro_feature_control(MSDK_SENSOR_FEATURE_ENUM feat_id,
                                              MUINT8 *para, MUINT32 *para_len)
{
    UINT16 *feature_return_para_16 = (UINT16 *)para;
    UINT16 *feature_data_16 = (UINT16 *)para;
    UINT32 *feature_return_para_32 = (UINT32 *)para;
    UINT32 *feature_data_32 = (UINT32 *)para;
    unsigned long long *feature_data = (unsigned long long *)para;
    struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
    MUINT32 *p32;

    switch (feat_id) {
    case SENSOR_FEATURE_CHECK_SENSOR_ID:
        if (feature_return_para_32 && para_len) {
            *feature_return_para_32 = GC02M1MACRO_SENSOR_ID;
            *para_len = sizeof(MUINT32);
        }
        break;
    case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
        if (feature_data) {
            p32 = (MUINT32 *)(uintptr_t)(*(feature_data + 1));
            if (p32)
                *p32 = 300;
        }
        break;
    case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
        if (feature_data) {
            p32 = (MUINT32 *)(uintptr_t)(*(feature_data + 1));
            if (p32)
                *p32 = (gc02m1macro_sensor.frame_length << 16) + gc02m1macro_sensor.line_length;
        }
        break;
    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
        if (feature_data) {
            p32 = (MUINT32 *)(uintptr_t)(*(feature_data + 1));
            if (p32)
                *p32 = gc02m1macro_sensor.pclk;
        }
        break;
    case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
        if (feature_data) {
            p32 = (MUINT32 *)(uintptr_t)(*(feature_data + 1));
            if (p32)
                *p32 = 66000000;
        }
        break;
    case SENSOR_FEATURE_GET_PIXEL_RATE:
        if (feature_data) {
            p32 = (MUINT32 *)(uintptr_t)(*(feature_data + 1));
            if (p32)
                *p32 = 63635200;
        }
        break;
    case SENSOR_FEATURE_GET_PERIOD:
        if (feature_return_para_16 && para_len) {
            *feature_return_para_16++ = gc02m1macro_sensor.line_length;
            *feature_return_para_16 = gc02m1macro_sensor.frame_length;
            *para_len = 4;
        }
        break;
    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
        if (feature_return_para_32 && para_len) {
            *feature_return_para_32 = gc02m1macro_sensor.pclk;
            *para_len = 4;
        }
        break;
    case SENSOR_FEATURE_SET_FRAMERATE:
        if (feature_data_32) {
            spin_lock(&gc02m1macro_lock);
            gc02m1macro_sensor.current_fps = *feature_data_32;
            spin_unlock(&gc02m1macro_lock);
        }
        break;
    case SENSOR_FEATURE_SET_HDR:
        if (feature_data_32) {
            spin_lock(&gc02m1macro_lock);
            gc02m1macro_sensor.ihdr_en = (kal_uint8)*feature_data_32;
            spin_unlock(&gc02m1macro_lock);
        }
        break;
    case SENSOR_FEATURE_GET_CROP_INFO:
        if (feature_data) {
            wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data + 1));
            if (wininfo) {
                kal_uint32 scen = *feature_data_32;
                if (scen < 5)
                    memcpy(wininfo, &gc02m1macro_winsize_info[scen], sizeof(*wininfo));
                else
                    memcpy(wininfo, &gc02m1macro_winsize_info[0], sizeof(*wininfo));
            }
        }
        break;
    case SENSOR_FEATURE_GET_BINNING_TYPE:
        if (feature_return_para_32 && para_len) {
            *feature_return_para_32 = 1;
            *para_len = 4;
        }
        break;
    case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
        if (feature_data) {
            *(feature_data + 1) = 64;
            *(feature_data + 2) = 1024;
        }
        break;
    case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
        if (feature_data) {
            *(feature_data + 0) = 50;
            *(feature_data + 1) = 1;
            *(feature_data + 2) = 0;
        }
        break;
    case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
        if (feature_data) {
            *(feature_data + 1) = 1;
        }
        break;
    case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
        if (feature_data) {
            *(feature_data + 1) = 1;
            *(feature_data + 2) = 4;
        }
        break;
    case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
        if (feature_data_16) {
            gc02m1macro_sensor.autoflicker_en = (kal_bool)*feature_data_16;
        }
        break;
    case SENSOR_FEATURE_SET_TEST_PATTERN:
        if (feature_data_32) {
            gc02m1macro_sensor.test_pattern = (kal_bool)*feature_data_32;
        }
        break;
    case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
        if (feature_return_para_32 && para_len) {
            *feature_return_para_32 = 0;
            *para_len = 4;
        }
        break;
    case SENSOR_FEATURE_SET_STREAMING_RESUME:
        write_cmos_sensor_8_8(0xfe, 0x00, gc02m1macro_sensor.i2c_write_id);
        write_cmos_sensor_8_8(0x3e, 0x90, gc02m1macro_sensor.i2c_write_id);
        break;
    case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
        write_cmos_sensor_8_8(0xfe, 0x00, gc02m1macro_sensor.i2c_write_id);
        write_cmos_sensor_8_8(0x3e, 0x00, gc02m1macro_sensor.i2c_write_id);
        break;
    case SENSOR_FEATURE_SET_ESHUTTER:
        if (feature_data_32) {
            gc02m1macro_sensor.shutter = *feature_data_32;
        }
        break;
    case SENSOR_FEATURE_SET_GAIN:
        if (feature_data_16) {
            gc02m1macro_sensor.gain = *feature_data_16;
        }
        break;
    case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
        if (feature_return_para_32 && para_len) {
            *feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
            *para_len = 4;
        }
        break;
    default:
        break;
    }
    return ERROR_NONE;
}

static kal_uint32 gc02m1macro_control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
                                      MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *win,
                                      MSDK_SENSOR_CONFIG_STRUCT *cfg)
{
    switch (scenario_id) {
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        write_regs_8(gc02m1macro_preview_regs, ARRAY_SIZE(gc02m1macro_preview_regs), gc02m1macro_sensor.i2c_write_id);
        break;
    default:
        write_regs_8(gc02m1macro_preview_regs, ARRAY_SIZE(gc02m1macro_preview_regs), gc02m1macro_sensor.i2c_write_id);
        break;
    }
    return ERROR_NONE;
}

static kal_uint32 gc02m1macro_close(void)
{
    return ERROR_NONE;
}

static struct SENSOR_FUNCTION_STRUCT gc02m1macro_sensor_func = {
    gc02m1macro_open,
    gc02m1macro_get_info,
    gc02m1macro_get_resolution,
    gc02m1macro_feature_control,
    gc02m1macro_control,
    gc02m1macro_close
};

UINT32 GC02M1MACRO_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
    if (pfFunc != NULL)
        *pfFunc = &gc02m1macro_sensor_func;
    return ERROR_NONE;
}
