#pragma once

#include <QString>
#include <QtGlobal>

namespace FlirEIPClass
{
    const quint8 kIdentity = 0x01;
    const quint8 kPassThrough = 0x70;
    const quint8 kTcpIp = 0xF5;
    const quint8 kEthernet = 0xF6;
    const quint8 kPhysicalIO = 0x6F;
    const quint8 kCameraControl = 0x65;
    const quint8 kAlarmSettings = 0x6A;
}

namespace FlirEIPPassthroughService
{
    const quint8 kReadBool = 0x32;
    const quint8 kWriteBool = 0x33;
    const quint8 kReadInt32 = 0x34;
    const quint8 kWriteInt32 = 0x35;
    const quint8 kReadDouble = 0x36;
    const quint8 kWriteDouble = 0x37;
    const quint8 kReadAscii = 0x38;
    const quint8 kWriteAscii = 0x39;
}

namespace FlirEIPIdentityAttribute
{
    const quint8 kVendorNumber = 0x01;
    const quint8 kDeviceType = 0x02;
    const quint8 kSerialNumber = 0x06;
    const quint8 kProductName = 0x07;
}

namespace FlirEIPEthernetAttribute
{
    const quint8 kPhysicalAddress = 0x03;
}

namespace FlirEIPCameraControlAttribute
{
    const quint8 kOverlayGraphics = 0x09;
    const quint8 kOverlayLabel = 0x10;
    const quint8 kOverlayScale = 0x11;
    const quint8 kOverlayDatetime = 0x12;
    const quint8 kOverlayEmmisivity = 0x13;
    const quint8 kOverlayDistance = 0x14;
    const quint8 kOverlayReflectedTemp = 0x15;
    const quint8 kOverlayAtmosphericTemp = 0x16;
    const quint8 kOverlayRelativeHumidity = 0x17;
    const quint8 kOverlayLens = 0x18;
    const quint8 kOverlayMeasurementMask = 0x19;
}

namespace FlirDataType
{
    const QString Ascii("ascii");
    const QString Int("int");
    const QString Short("short");
    const QString Double("double");
    const QString Bool("bool");
}
