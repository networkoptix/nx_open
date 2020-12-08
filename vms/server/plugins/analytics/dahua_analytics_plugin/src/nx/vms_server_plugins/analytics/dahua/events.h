#pragma once

#include <vector>
#include <chrono>
#include <optional>

#include <QtCore/QString>

#include "objects.h"

namespace nx::vms_server_plugins::analytics::dahua {

struct EventTypeGroup
{
    QString id;
    QString prettyName;

    static const EventTypeGroup kBasic;
    static const EventTypeGroup kSimpleAnalytics;
    static const EventTypeGroup kComplexAnalytics;
    static const EventTypeGroup kAudio;
    static const EventTypeGroup kAlarm;
    static const EventTypeGroup kSystem;
};

extern const std::vector<const EventTypeGroup*>& kEventTypeGroups;

struct EventType
{
    QString nativeId;
    QString id;
    QString prettyName;
    QString description;
    bool isStateDependent = false;
    bool isRegionDependent = false;
    const EventTypeGroup* group = nullptr;
    std::vector<const ObjectType*> objectTypes;

    static const EventType kVideoMotion;
    static const EventType kVideoLoss;
    static const EventType kVideoBlind;
    static const EventType kVideoUnFocus;
    static const EventType kVideoAbnormalDetection;
    static const EventType kCrossLineDetection;
    static const EventType kCrossRegionDetection;
    static const EventType kLeftDetection;
    static const EventType kTakenAwayDetection;
    static const EventType kFaceDetection;
    static const EventType kAudioMutation;
    static const EventType kAudioAnomaly;
    static const EventType kStorageNotExist;
    static const EventType kStorageFailure;
    static const EventType kStorageLowSpace;
    static const EventType kHeatImagingTemper;
    static const EventType kLoginFailure;
    static const EventType kAlarmLocal;
    static const EventType kAlarmOutput;
    static const EventType kCrowdDetection;
    static const EventType kParkingDetection;
    static const EventType kRioterDetection;
    static const EventType kMoveDetection;
    static const EventType kWanderDetection;
    static const EventType kStayDetection;
    static const EventType kQueueStayDetection;

    static const EventType* findById(const QString& id);
    static const EventType* findByNativeId(const QString& nativeId);
};

extern const std::vector<const EventType*>& kEventTypes;

struct Event
{
    const EventType* type = nullptr;
    std::optional<unsigned int> sequenceId;
    std::optional<std::chrono::milliseconds> timestamp;
    bool isActive = false;
    std::optional<QString> ruleName;
    std::vector<Object> objects;
};

} // namespace nx::vms_server_plugins::analytics::dahua
