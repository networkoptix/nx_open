#include "events.h"

#include <map>

#include <QtCore/QStringView>

namespace nx::vms_server_plugins::analytics::dahua {

namespace {

// 4.11.10 Find media files with IVS info
const std::vector<const ObjectType*> ivsObjectTypes = {
    &ObjectType::kUnknown,
    &ObjectType::kNonMotor,
    &ObjectType::kVehicle,
    &ObjectType::kHuman,
};

} // namespace

static std::vector<const EventTypeGroup*> eventTypeGroups;

#define NX_DEFINE_EVENT_TYPE_GROUP(NAME) \
    namespace { \
    \
    struct NAME##EventTypeGroup: EventTypeGroup \
    { \
        NAME##EventTypeGroup(); \
    }; \
    \
    } /* namespace */ \
    \
    const EventTypeGroup EventTypeGroup::k##NAME = \
        (eventTypeGroups.push_back(&k##NAME), NAME##EventTypeGroup()); \
    \
    NAME##EventTypeGroup::NAME##EventTypeGroup()
        
NX_DEFINE_EVENT_TYPE_GROUP(Basic)
{
    id = "nx.dahua.group.BasicEvents";
    prettyName = "Basic events";
}

NX_DEFINE_EVENT_TYPE_GROUP(SimpleAnalytics)
{
    id = "nx.dahua.group.SimpleAnalyticsEvents",
    prettyName = "Simple analytics events";
}

NX_DEFINE_EVENT_TYPE_GROUP(ComplexAnalytics)
{
    id = "nx.dahua.group.ComplexAnalyticsEvents",
    prettyName = "Complex analytics events";
}

NX_DEFINE_EVENT_TYPE_GROUP(Audio)
{
    id = "nx.dahua.group.AudioEvents",
    prettyName = "Audio events";
}

NX_DEFINE_EVENT_TYPE_GROUP(Alarm)
{
    id = "nx.dahua.group.AlarmEvents",
    prettyName = "Alarm events";
}

NX_DEFINE_EVENT_TYPE_GROUP(System)
{
    id = "nx.dahua.group.SystemEvents",
    prettyName = "System events";
}

const std::vector<const EventTypeGroup*>& kEventTypeGroups = eventTypeGroups;

static std::vector<const EventType*> eventTypes;

#define NX_DEFINE_EVENT_TYPE(NAME) \
    namespace { \
    \
    struct NAME##EventType: EventType \
    { \
        NAME##EventType(); \
    }; \
    \
    } /* namespace */ \
    \
    const EventType EventType::k##NAME = \
        (eventTypes.push_back(&k##NAME), NAME##EventType()); \
    \
    NAME##EventType::NAME##EventType()

NX_DEFINE_EVENT_TYPE(VideoMotion)
{
    nativeId = "VideoMotion";
    id = "nx.dahua.VideoMotion";
    prettyName = "Motion detection";
    description = "Video motion";
    isStateDependent = true;
    isRegionDependent = true;
    group = &EventTypeGroup::kBasic;
}

NX_DEFINE_EVENT_TYPE(VideoLoss)
{
    nativeId = "VideoLoss";
    id = "nx.dahua.VideoLoss";
    prettyName = "Video loss detection";
    description = "Video loss";
    isStateDependent = true;
    group = &EventTypeGroup::kBasic;
}

NX_DEFINE_EVENT_TYPE(VideoBlind)
{
    nativeId = "VideoBlind";
    id = "nx.dahua.VideoBlind";
    prettyName = "Video blind detection";
    description = "Video blind";
    isStateDependent = true;
    group = &EventTypeGroup::kBasic;
}

NX_DEFINE_EVENT_TYPE(VideoUnFocus)
{
    nativeId = "VideoUnFocus";
    id = "nx.dahua.VideoUnFocus";
    prettyName = "Defocus detection";
    description = "Defocus";
    isStateDependent = true;
    group = &EventTypeGroup::kBasic;
}

NX_DEFINE_EVENT_TYPE(VideoAbnormalDetection)
{
    nativeId = "VideoAbnormalDetection";
    id = "nx.dahua.VideoAbnormalDetection";
    prettyName = "Scene change detection (Video abnormal detection)";
    description = "Scene change";
    isStateDependent = true;
    group = &EventTypeGroup::kBasic;
}

NX_DEFINE_EVENT_TYPE(CrossLineDetection)
{
    nativeId = "CrossLineDetection";
    id = "nx.dahua.CrossLineDetection";
    prettyName = "Tripwire detection (Cross line detection)";
    description = "Cross line";
    isStateDependent = true;
    isRegionDependent = true;
    group = &EventTypeGroup::kSimpleAnalytics;
    objectTypes = ivsObjectTypes;
}

NX_DEFINE_EVENT_TYPE(CrossRegionDetection)
{
    nativeId = "CrossRegionDetection";
    id = "nx.dahua.CrossRegionDetection";
    prettyName = "Intrusion detection (Cross region detection)";
    description = "Intrusion";
    isStateDependent = true;
    isRegionDependent = true;
    group = &EventTypeGroup::kSimpleAnalytics;
    objectTypes = ivsObjectTypes;
}

NX_DEFINE_EVENT_TYPE(LeftDetection)
{
    nativeId = "LeftDetection";
    id = "nx.dahua.LeftDetection";
    prettyName = "Abandoned object detection (Left object detection)";
    description = "Abandoned object";
    isStateDependent = true;
    isRegionDependent = true;
    group = &EventTypeGroup::kSimpleAnalytics;
    objectTypes = ivsObjectTypes;
}

NX_DEFINE_EVENT_TYPE(TakenAwayDetection)
{
    nativeId = "TakenAwayDetection";
    id = "nx.dahua.TakenAwayDetection";
    prettyName = "Missing object detection (Taken away detection)";
    description = "Taken away object";
    isStateDependent = true;
    isRegionDependent = true;
    group = &EventTypeGroup::kSimpleAnalytics;
    objectTypes = ivsObjectTypes;
}

NX_DEFINE_EVENT_TYPE(FaceDetection)
{
    nativeId = "FaceDetection";
    id = "nx.dahua.FaceDetection";
    prettyName = "Face detection";
    description = "Face";
    isStateDependent = true;
    group = &EventTypeGroup::kSimpleAnalytics;
    objectTypes = {&ObjectType::kHumanFace};
}

NX_DEFINE_EVENT_TYPE(AudioMutation)
{
    nativeId = "AudioMutation";
    id = "nx.dahua.AudioMutation";
    prettyName = "Audio intensity change detection (Audio mutation detection)";
    description = "Audio intensity change";
    isStateDependent = true;
    group = &EventTypeGroup::kAudio;
}

NX_DEFINE_EVENT_TYPE(AudioAnomaly)
{
    nativeId = "AudioAnomaly";
    id = "nx.dahua.AudioAnomaly";
    prettyName = "Audio anomaly detection (Audio input abnormal detection)";
    description = "Audio input abnormal";
    isStateDependent = true;
    group = &EventTypeGroup::kAudio;
}

NX_DEFINE_EVENT_TYPE(StorageNotExist)
{
    nativeId = "StorageNotExist";
    id = "nx.dahua.StorageNotExist";
    prettyName = "Storage absence detection";
    description = "Storage absence";
    isStateDependent = true;
    group = &EventTypeGroup::kSystem;
}

NX_DEFINE_EVENT_TYPE(StorageFailure)
{
    nativeId = "StorageFailure";
    id = "nx.dahua.StorageFailure";
    prettyName = "Storage failure detection";
    description = "Storage failure";
    isStateDependent = true;
    group = &EventTypeGroup::kSystem;
}

NX_DEFINE_EVENT_TYPE(StorageLowSpace)
{
    nativeId = "StorageLowSpace";
    id = "nx.dahua.StorageLowSpace";
    prettyName = "Storage low space detection";
    description = "Storage low space";
    isStateDependent = true;
    group = &EventTypeGroup::kSystem;
}

NX_DEFINE_EVENT_TYPE(HeatImagingTemper)
{
    nativeId = "HeatImagingTemper";
    id = "nx.dahua.HeatImagingTemper";
    prettyName = "High temperature detection";
    description = "High temperature";
    isStateDependent = true;
    group = &EventTypeGroup::kSystem;
}

NX_DEFINE_EVENT_TYPE(LoginFailure)
{
    nativeId = "LoginFailure";
    id = "nx.dahua.LoginFailure";
    prettyName = "Login error detection";
    description = "Login error detected";
    group = &EventTypeGroup::kSystem;
}

NX_DEFINE_EVENT_TYPE(AlarmLocal)
{
    nativeId = "AlarmLocal";
    id = "nx.dahua.AlarmLocal";
    prettyName = "Alarm detection";
    description = "Alarm local";
    isStateDependent = true;
    group = &EventTypeGroup::kAlarm;
}

NX_DEFINE_EVENT_TYPE(AlarmOutput)
{
    nativeId = "AlarmOutput";
    id = "nx.dahua.AlarmOutput";
    prettyName = "Alarm output detection";
    description = "Alarm local";
    isStateDependent = true;
    group = &EventTypeGroup::kAlarm;
}

NX_DEFINE_EVENT_TYPE(CrowdDetection)
{
    nativeId = "CrowdDetection";
    id = "nx.dahua.CrowdDetection";
    prettyName = "Crowd detection (Crowd density overrun detection)";
    description = "Crowd dencity overrun";
    isStateDependent = true;
    group = &EventTypeGroup::kComplexAnalytics;
}

NX_DEFINE_EVENT_TYPE(ParkingDetection)
{
    nativeId = "ParkingDetection";
    id = "nx.dahua.ParkingDetection";
    prettyName = "Parking detection";
    description = "Parking";
    isStateDependent = true;
    group = &EventTypeGroup::kComplexAnalytics;
}

NX_DEFINE_EVENT_TYPE(RioterDetection)
{
    nativeId = "RioterDetection";
    id = "nx.dahua.RioterDetection";
    prettyName = "Rioter detection (People gathering detection)";
    description = "People gathering";
    isStateDependent = true;
    group = &EventTypeGroup::kComplexAnalytics;
    objectTypes = ivsObjectTypes;
}

NX_DEFINE_EVENT_TYPE(MoveDetection)
{
    nativeId = "MoveDetection";
    id = "nx.dahua.MoveDetection";
    prettyName = "Fast moving detection";
    description = "Fast moving";
    isStateDependent = true;
    group = &EventTypeGroup::kSimpleAnalytics;
    objectTypes = ivsObjectTypes;
}

NX_DEFINE_EVENT_TYPE(WanderDetection)
{
    nativeId = "WanderDetection";
    id = "nx.dahua.WanderDetection";
    prettyName = "Wander detection (Loitering detection)";
    description = "Loitering";
    isStateDependent = true;
    isRegionDependent = true;
    group = &EventTypeGroup::kComplexAnalytics;
    objectTypes = ivsObjectTypes;
}

NX_DEFINE_EVENT_TYPE(StayDetection)
{
    nativeId = "StayDetection";
    id = "nx.dahua.StayDetection";
    prettyName = "Stay detection";
    description = "Stay";
    isStateDependent = true;
    isRegionDependent = true;
    group = &EventTypeGroup::kComplexAnalytics;
    objectTypes = ivsObjectTypes;
}

NX_DEFINE_EVENT_TYPE(QueueStayDetection)
{
    nativeId = "QueueStayDetection";
    id = "nx.dahua.QueueStayDetection";
    prettyName = "Queue stay detection";
    description = "Queue stay";
    isStateDependent = true;
    isRegionDependent = true;
    group = &EventTypeGroup::kComplexAnalytics;
}

#undef NX_DEFINE_EVENT_TYPE

static const auto eventTypeById =
    []()
    {
        std::map<QStringView, const EventType*> map;

        for (const auto type: eventTypes)
            map[type->id] = type;

        return map;
    }();

const EventType* EventType::findById(const QString& id)
{
    if (const auto it = eventTypeById.find(id); it != eventTypeById.end())
        return it->second;

    return nullptr;
}

static const auto eventTypeByNativeId =
    []()
    {
        std::map<QStringView, const EventType*> map;

        for (const auto type: eventTypes)
            map[type->nativeId] = type;

        return map;
    }();

const EventType* EventType::findByNativeId(const QString& nativeId)
{
    if (const auto it = eventTypeByNativeId.find(nativeId); it != eventTypeByNativeId.end())
        return it->second;

    return nullptr;
}

const std::vector<const EventType*>& kEventTypes = eventTypes;

} // namespace nx::vms_server_plugins::analytics::dahua
