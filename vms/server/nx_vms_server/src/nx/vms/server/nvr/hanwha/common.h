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

} // namespace nx::vms::server::nvr::hanwha
