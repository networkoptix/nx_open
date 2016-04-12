#ifndef FLIR_EIP_DATA_H
#define FLIR_EIP_DATA_H

#include <QString>
#include <QtGlobal>

namespace FlirEIPClass
{
    const quint8 IDENTITY = 0x01;
    const quint8 PASSTHROUGH = 0x70;
    const quint8 TCP_IP = 0xF5;
    const quint8 ETHERNET = 0xF6;
    const quint8 PHYSICAL_IO = 0x6F;
    const quint8 CAMERA_CONTROL = 0x65;
}

namespace FlirEIPPassthroughService
{
    const quint8 READ_BOOL = 0x32;
    const quint8 WRITE_BOOL = 0x33;
    const quint8 READ_INT32 = 0x34;
    const quint8 WRITE_INT32 = 0x35;
    const quint8 READ_DOUBLE = 0x36;
    const quint8 WRITE_DOUBLE = 0x37;
    const quint8 READ_ASCII = 0x38;
    const quint8 WRITE_ASCII = 0x39;
}

namespace FlirEIPIdentityAttribute
{
    const quint8 VENDOR_NUMBER = 0x01;
    const quint8 DEVICE_TYPE = 0x02;
    const quint8 SERIAL_NUMBER = 0x06;
    const quint8 PRODUCT_NAME = 0x07;
}

namespace FlirEIPEthernetAttribute
{
    const quint8 PHYSICAL_ADDRESS = 0x03;
}

namespace FlirEIPCameraControlAttribute
{
    const quint8 OVERLAY_GRAPHICS = 0x09;
    const quint8 OVERLAY_LABEL = 0x10;
    const quint8 OVERLAY_SCALE = 0x11;
    const quint8 OVERLAY_DATETIME = 0x12;
    const quint8 OVERLAY_EMISSIVITY = 0x13;
    const quint8 OVERLAY_DISTANCE = 0x14;
    const quint8 OVERLAY_REFLECTED_TEMP = 0x15;
    const quint8 OVERLAY_ATMOSPHERIC_TEMP = 0x16;
    const quint8 OVERLAY_RELATVE_HUMIDITY = 0x17;
    const quint8 OVERLAY_LENS = 0x18;
    const quint8 OVERLAY_MESUREMENT_MASK = 0x19;
}

namespace FlirDataType
{
    const QString Ascii("ascii");
    const QString Int("int");
    const QString Short("short");
    const QString Double("double");
    const QString Bool("bool");
}

#endif // FLIR_EIP_DATA_H
