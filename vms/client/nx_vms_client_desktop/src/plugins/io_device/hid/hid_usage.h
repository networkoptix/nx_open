#pragma once

namespace nx {
namespace client {
namespace plugins {
namespace io_device {
namespace hid {
namespace usage {
namespace generic_desktop {

const uint16_t kUndefined = 0x00;
const uint16_t kPointer = 0x01;
const uint16_t kMouse = 0x02;
const uint16_t kJoystick = 0x04;
const uint16_t kGamePad = 0x05;
const uint16_t kKeyboard = 0x06;
const uint16_t kKeypad = 0x07;
const uint16_t kMultiAxisController = 0x08;
const uint16_t kTabletPcSystemControls = 0x09;
const uint16_t kX = 0x30;
const uint16_t kY = 0x31;
const uint16_t kZ = 0x32;
const uint16_t kRx = 0x33;
const uint16_t kRy = 0x34;
const uint16_t kRz = 0x35;
const uint16_t kSlider = 0x36;
const uint16_t kDial = 0x37;
const uint16_t kWheel = 0x38;
const uint16_t kHatSwitch = 0x39;
const uint16_t kCountedBuffer = 0x3a;
const uint16_t kByteCount = 0x3b;
const uint16_t kMotionWakeup = 0x3c;
const uint16_t kStart = 0x3d;
const uint16_t kSelect = 0x3e;
const uint16_t kVx = 0x40;
const uint16_t kVy = 0x41;
const uint16_t kVz = 0x42;
const uint16_t kVbrX = 0x43;
const uint16_t kVbrY = 0x44;
const uint16_t kVbrZ = 0x45;
const uint16_t kVno = 0x46;
const uint16_t kFeatureNotification = 0x47;
const uint16_t kResolutionMultiplier = 0x48;
const uint16_t kSystemControl = 0x80;
const uint16_t kSystemPowerDown = 0x81;
const uint16_t kSystemSleep = 0x82;
const uint16_t kSystemWakeup = 0x83;
const uint16_t kSystemContextMenu = 0x84;
const uint16_t kSystemMainMenu = 0x85;
const uint16_t kSystemAppMenu = 0x86;
const uint16_t kSystemMenuHelp = 0x87;
const uint16_t kSystemMenuExit = 0x88;
const uint16_t kSystemMenuSelect = 0x89;
const uint16_t kSystemMenuRight = 0x8a;
const uint16_t kSystemMenuLeft = 0x8b;
const uint16_t kSystemMenuUp = 0x8c;
const uint16_t kSystemMenuDown = 0x8d;
const uint16_t kSystemColdRestart = 0x8e;
const uint16_t kSystemWarmRestart = 0x8f;
const uint16_t kDpadUp = 0x90;
const uint16_t kDpadDown = 0x91;
const uint16_t kDpadRight = 0x92;
const uint16_t kDpadLeft = 0x93;
const uint16_t kSystemDock = 0xa0;
const uint16_t kSystemUndock = 0xa1;
const uint16_t kSystemSetup = 0xa2;
const uint16_t kSystemBreak = 0xa3;
const uint16_t kSystemDebuggerBreak = 0xa4;
const uint16_t kApplicationBreak = 0xa5;
const uint16_t kApplicationDebuggerBreak = 0xa6;
const uint16_t kSystemSpeakerMute = 0xa7;
const uint16_t kSystemHibernate = 0xa8;
const uint16_t kSystemDisplayInvert = 0xb0;
const uint16_t kSystemDisplayInternal = 0xb1;
const uint16_t kSystemDisplayExternal = 0xb2;
const uint16_t kSystemDisplayBoth = 0xb3;
const uint16_t kSystemDisplayDual = 0xb4;
const uint16_t kSystemDisplayToggleIntExt = 0xb5;
const uint16_t kSystemDisplaySwapPrimarySecondary = 0xb6;
const uint16_t kSystemLcdAutoscale = 0xb7;

} // namespace generic_desktop
} // namespace usage
} // namespace hid
} // namespace io_device
} // namespace plugins
} // namespace client
} // namespace nx