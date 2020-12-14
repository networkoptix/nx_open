#pragma once

#include <vector>
#include <chrono>
#include <optional>

#include <QtCore/QString>

#include "attributes.h"
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

    // Some cameras declare support for some events using these rather nativeId.
    std::vector<QString> aliasNativeIds;

    QString id;
    QString prettyName;
    QString description;
    bool isStateDependent = true;
    const EventTypeGroup* group = nullptr;
    std::function<void(std::vector<Attribute>*, const QJsonObject&)> parseAttributes;
    std::vector<const ObjectType*> objectTypes;
    std::function<std::vector<QJsonObject>(const QJsonObject&)> extractObjects;

    // Some event types violate documented coordinate domain. This value is used to override the
    // documented default.
    std::optional<float> coordinateDomain;

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
    static const EventType kNumberStat;
    static const EventType kManNumDetection;
    static const EventType kStayDetection;
    static const EventType kQueueNumDetection;
    static const EventType kQueueStayDetection;

    static const EventType* findById(const QString& id);
    static const EventType* findByNativeId(const QString& nativeId);
};

extern const std::vector<const EventType*>& kEventTypes;

struct Event
{
    const EventType* type = nullptr;
    std::chrono::milliseconds timestamp;
    unsigned int id = 0;
    bool isActive = false;
    std::optional<QString> ruleName;
    std::optional<unsigned int> areaId;
    std::vector<Attribute> attributes;
    std::vector<Object> objects;
};

} // namespace nx::vms_server_plugins::analytics::dahua
