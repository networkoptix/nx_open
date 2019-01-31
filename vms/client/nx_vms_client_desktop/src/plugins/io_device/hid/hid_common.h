#pragma once

#include "hid_usage.h"

namespace nx {
namespace client {
namespace plugins {
namespace io_device {
namespace hid {

enum class UsagePage
{
    undefined = 0x00,
    genericDesktop = 0x01,
    simulation = 0x02,
    vr = 0x03,
    sport = 0x04,
    game = 0x05,
    genericDevice = 0x06,
    keyboardOrKeypad = 0x07,
    leds = 0x08,
    button = 0x09,
    ordinal = 0x0a,
    telephony = 0x0b,
    consumer = 0x0c,
    digitizer = 0x0c,
    pidPage = 0x0f,
    unicode = 0x10,
    alphanumericDisplay = 0x14,
    medicalInstruments = 0x40,
    barCodeScanner = 0x8c,
    scale = 0x8d,
    magneticStripeReader = 0x8e,
    pointOfSale = 0x8f,
    cameraControl = 0x90,
    arcade = 0x91
};

bool isReservedUsagePage(quint16 pageCode);

bool isVendorDefinedUsagePage(quint16 pageCode);

bool isPowerUsagePage(quint16 pageCode);

bool isMonitorUsagePage(quint16 pageCode);

} // namespace hid
} // namespace io_device
} // namespace plugins
} // namespace client 
} // namespace nx