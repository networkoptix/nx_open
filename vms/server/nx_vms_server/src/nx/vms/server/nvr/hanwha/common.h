#pragma once

namespace nx::vms::server::nvr::hanwha {

static const QString kInputIdPrefix("DI");
static const QString kOutputIdPrefix("DO");

static constexpr int kInputCount = 4;
static constexpr int kOutputCount = 2;

static const QString kDefaultVendor("Hanwha");
static const QString kHanwhaPoeNvrDriverName("Hanwha WAVE PoE NVR");
static const QString kRecordingLedId("recordingLed");
static const QString kPoeOverBudgetLedId("poeOverBudgetLed");
static const QString kAlarmOutputLedId("alarmOutputLed");

static const QString kIoDeviceFileName("/dev/ia_resource");
static const QString kNetworkControllerDeviceFileName("/dev/ip1829_cdev");
static const QString kPowerSupplyDeviceFileName("/dev/poe");

static const int kWrn1610BoardId = 0;
static const int kWrn810BoardId = 0b1;
static const int kWrn410BoardId = 0b11;

} // namespace nx::vms::server::nvr::hanwha
