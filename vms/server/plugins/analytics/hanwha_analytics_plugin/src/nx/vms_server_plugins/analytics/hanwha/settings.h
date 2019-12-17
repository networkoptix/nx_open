#pragma once

#include "point.h"

#include <array>
#include <vector>
#include <memory>

#include <nx/kit/json.h>

#include <nx/sdk/i_string_map.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/helpers/settings_response.h>

namespace nx::vms_server_plugins::analytics::hanwha {

//-------------------------------------------------------------------------------------------------

enum class EventCategory
{
    motionDetection,
    faceDetectionGeneral, //< not supported
    tampering,
    audioDetection,
    defocusDetection,
    fogDetection, //< not supported
    videoAnalytics, //< includes Passing, Intrusion, Entering, Exiting, Appearing, Loitering
    audioAnalytics, //< includes Scream, Gunshot, Explosion, GlassBreak
    queues, //< not supported //< include queue1, queue1, queue3
    shockDetection,
    objectDetection, //< includes Person, Vehicle, Face, LicensePlate
    count //< number event of categories
};

using SupportedEventCategories = std::array<bool, int(EventCategory::count)>;
//-------------------------------------------------------------------------------------------------

struct SettingGroup
{
    // Server contains a map, where the values of all settings are stored. The keys to values, that
    // correspond to the current group of settings, are stored in the string array `serverKeys`.
    // Both `serverKeys` and `serverKeyCount` are initialized by descendants of this class.
    const char* const* const serverKeys;
    const int serverKeyCount;

    template<int N>
    SettingGroup(
        const char* const(&paramsArray)[N],
        int nativeIndex = -1,
        int serverIndexOffset = 0,
        int deviceIndexOffset = 0)
    :
        serverKeys(paramsArray),
        serverKeyCount(N),
        m_nativeIndex(nativeIndex),
        m_serverIndex(nativeIndex >= 0 ? nativeIndex + serverIndexOffset : -1),
        m_deviceIndex(nativeIndex >= 0 ? nativeIndex + deviceIndexOffset : -1)
    {
    }

    SettingGroup& operator=(const SettingGroup& rhs)
    {
        initialized = rhs.initialized;
        m_nativeIndex = rhs.m_nativeIndex;
        m_serverIndex = rhs.m_serverIndex;
        m_deviceIndex = rhs.m_deviceIndex;
        return *this;
    }
    operator bool() const { return initialized; }

    std::string serverKey(int keyIndex) const;

    void replanishErrorMap(
        nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const;

    int nativeIndex() const { return m_nativeIndex; }
    int serverIndex() const { return m_serverIndex; }
    int deviceIndex() const { return m_deviceIndex; }

protected:
    bool initialized = false;

private:

    // Index of the current SettingGroup, if several same type groups are used in the device.
    // E.g. if this group of settings describes a line, and many lines are supported, then
    // it is a number of the line (beginning from 0).
    // - 1 means that only one such settings group exist.
    int m_nativeIndex = -1;

    int m_serverIndex = -1;
    int m_deviceIndex = -1;
};

//-------------------------------------------------------------------------------------------------

struct ShockDetection: public SettingGroup
{
    bool enabled = false;
    int thresholdLevel = 50;
    int sensitivityLevel = 80;

    enum class ServerParamIndex {
        enabled,
        thresholdLevel,
        sensitivityLevel,
    };
    static constexpr const char* kServerKeys[] = {
        "ShockDetection.Enable",
        "ShockDetection.ThresholdLevel",
        "ShockDetection.SensitivityLevel",
    };
    static constexpr const char* kJsonEventName = "ShockDetection";
    static constexpr const char* kSunapiEventName = "shockdetection";

    ShockDetection(int /*roiIndex*/ = -1): SettingGroup(kServerKeys) {}
    bool operator==(const ShockDetection& rhs) const;
    bool operator!=(const ShockDetection& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    // The following functions perhaps should be moved to the inheritor-class.
    // The idea is that the current class interacts with the server only,
    // and the inheritor interacts with the device.
    // The decision will be made during other plugins construction.
    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize /*frameSize*/);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;
};

//-------------------------------------------------------------------------------------------------

struct Motion: public SettingGroup
{
    std::string detectionType;

    enum class ServerParamIndex {
        detectionType,
    };
    static constexpr const char* kServerKeys[] = {
        "MotionDetection.DetectionType",
    };
    static constexpr const char* kJsonEventName = "VideoAnalysis";
    static constexpr const char* kSunapiEventName = "videoanalysis2";

    Motion(int /*roiIndex*/ = -1): SettingGroup(kServerKeys) {}
    bool operator==(const Motion& rhs) const;
    bool operator!=(const Motion& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize /*frameSize*/);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;
};

//-------------------------------------------------------------------------------------------------

struct MotionDetectionObjectSize: public SettingGroup
{
    Width minWidth;
    Height minHeight;

    Width maxWidth;
    Height maxHeight;

    enum class ServerParamIndex {
        minWidth,
        minHeight,
        maxWidth,
        maxHeight,
    };
    static constexpr const char* kServerKeys[] = {
        "MotionDetection.MinObjectSize.Points",
        "MotionDetection.MinObjectSize.Points", //< the same as previous
        "MotionDetection.MaxObjectSize.Points",
        "MotionDetection.MaxObjectSize.Points", //< the same as previous
    };
    static constexpr const char* kJsonEventName = "VideoAnalysis";
    static constexpr const char* kSunapiEventName = "videoanalysis2";

    MotionDetectionObjectSize(int /*roiIndex*/ = -1): SettingGroup(kServerKeys) {}
    bool operator == (const MotionDetectionObjectSize& rhs) const;
    bool operator!=(const MotionDetectionObjectSize& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;

private:
    std::string buildMinObjectSize(FrameSize frameSize) const;
    std::string buildMaxObjectSize(FrameSize frameSize) const;
};

//-------------------------------------------------------------------------------------------------

struct IvaObjectSize: public SettingGroup
{
    Width minWidth;
    Height minHeight;

    Width maxWidth;
    Height maxHeight;

    enum class ServerParamIndex {
        minWidth,
        minHeight,
        maxWidth,
        maxHeight,
    };
    static constexpr const char* kServerKeys[] = {
        "IVA.MinObjectSize.Points",
        "IVA.MinObjectSize.Points", //< the same as previous, we read corresponding value twice
        "IVA.MaxObjectSize.Points",
        "IVA.MaxObjectSize.Points", //< the same as previous, we read corresponding value twice
    };
    static constexpr const char* kJsonEventName = "VideoAnalysis";
    static constexpr const char* kSunapiEventName = "videoanalysis2";

    IvaObjectSize(int /*roiIndex*/ = -1): SettingGroup(kServerKeys) {}
    bool operator == (const IvaObjectSize& rhs) const;
    bool operator!=(const IvaObjectSize& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    //void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;

private:
    std::string buildMinObjectSize(FrameSize frameSize) const;
    std::string buildMaxObjectSize(FrameSize frameSize) const;
};

//-------------------------------------------------------------------------------------------------

struct MotionDetectionIncludeArea: public SettingGroup
{
    std::vector<PluginPoint> points;
    int thresholdLevel = 50;
    int sensitivityLevel = 80;
    int minimumDuration = 0;

    static constexpr int kStartServerRoiIndexFrom = 1;
    static constexpr int kStartDeviceRoiIndexFrom = 1;

    enum class ServerParamIndex {
        points,
        thresholdLevel,
        sensitivityLevel,
        minimumDuration,
    };
    static constexpr const char* kServerKeys[] = {
        "MotionDetection.IncludeArea#.Points",
        "MotionDetection.IncludeArea#.ThresholdLevel",
        "MotionDetection.IncludeArea#.SensitivityLevel",
        "MotionDetection.IncludeArea#.MinimumDuration",
    };
    static constexpr const char* kJsonEventName = "VideoAnalysis";
    static constexpr const char* kSunapiEventName = "videoanalysis2";

    MotionDetectionIncludeArea(int roiIndex = -1):
    SettingGroup(kServerKeys, roiIndex, kStartServerRoiIndexFrom, kStartDeviceRoiIndexFrom)
    {
    }
    bool operator==(const MotionDetectionIncludeArea& rhs) const;
    bool operator!=(const MotionDetectionIncludeArea& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int roiIndex);
    //void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;
};

//-------------------------------------------------------------------------------------------------

struct MotionDetectionExcludeArea: public SettingGroup
{
    std::vector<PluginPoint> points;

    static constexpr int kStartServerRoiIndexFrom = 1;
    static constexpr int kStartDeviceRoiIndexFrom = 9;

    enum class ServerParamIndex {
        points,
    };
    static constexpr const char* kServerKeys[] = {
        "MotionDetection.ExcludeArea#.Points",
    };
    static constexpr const char* kJsonEventName = "VideoAnalysis";
    static constexpr const char* kSunapiEventName = "videoanalysis2";

    MotionDetectionExcludeArea(int roiIndex = -1):
    SettingGroup(kServerKeys, roiIndex, kStartServerRoiIndexFrom, kStartDeviceRoiIndexFrom)
    {
    }

    bool operator == (const MotionDetectionExcludeArea& rhs) const;
    bool operator!=(const MotionDetectionExcludeArea& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int roiIndex);
    //void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;
};

//-------------------------------------------------------------------------------------------------

struct TamperingDetection: SettingGroup
{
    bool enabled = false;
    int thresholdLevel = 50;
    int sensitivityLevel = 80;
    int minimumDuration = 0;
    bool exceptDarkImages = false;

    enum class ServerParamIndex {
        enabled,
        thresholdLevel,
        sensitivityLevel,
        minimumDuration,
        exceptDarkImages,
    };
    static constexpr const char* kServerKeys[] = {
        "TamperingDetection.Enable",
        "TamperingDetection.ThresholdLevel",
        "TamperingDetection.SensitivityLevel",
        "TamperingDetection.MinimumDuration",
        "TamperingDetection.ExceptDarkImages",
    };
    static constexpr const char* kJsonEventName = "TamperingDetection";
    static constexpr const char* kSunapiEventName = "tamperingdetection";

    TamperingDetection(int /*roiIndex*/ = -1): SettingGroup(kServerKeys) {}
    bool operator==(const TamperingDetection& rhs) const;
    bool operator!=(const TamperingDetection& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    //void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;
};

//-------------------------------------------------------------------------------------------------

struct DefocusDetection: SettingGroup
{
    bool enabled = false;
    int thresholdLevel = 50;
    int sensitivityLevel = 80;
    int minimumDuration = 0;

    enum class ServerParamIndex {
        enabled,
        thresholdLevel,
        sensitivityLevel,
        minimumDuration,
    };
    static constexpr const char* kServerKeys[] = {
        "DefocusDetection.Enable",
        "DefocusDetection.ThresholdLevel",
        "DefocusDetection.SensitivityLevel",
        "DefocusDetection.MinimumDuration",
    };
    static constexpr const char* kJsonEventName = "DefocusDetection";
    static constexpr const char* kSunapiEventName = "defocusdetection";

    DefocusDetection(int /*roiIndex*/ = -1): SettingGroup(kServerKeys) {}
    bool operator==(const DefocusDetection& rhs) const;
    bool operator!=(const DefocusDetection& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    //void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;
};

//-------------------------------------------------------------------------------------------------

struct FogDetection: public SettingGroup
{
    bool enabled = false;
    int thresholdLevel = 50;
    int sensitivityLevel = 80;
    int minimumDuration = 30;

    enum class ServerParamIndex {
        enabled,
        thresholdLevel,
        sensitivityLevel,
        minimumDuration,
    };
    static constexpr const char* kServerKeys[] = {
        "FogDetection.Enable",
        "FogDetection.ThresholdLevel",
        "FogDetection.SensitivityLevel",
        "FogDetection.MinimumDuration",
    };
    static constexpr const char* kJsonEventName = "FogDetection";
    static constexpr const char* kSunapiEventName = "fogdetection";

    FogDetection(int /*roiIndex*/ = -1) : SettingGroup(kServerKeys) {}
    bool operator==(const FogDetection& rhs) const;
    bool operator!=(const FogDetection& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    //void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;

};
//-------------------------------------------------------------------------------------------------

struct FaceDetectionGeneral: public SettingGroup
{
    bool enabled = false;
    int sensitivityLevel = 80;

    enum class ServerParamIndex {
        enabled,
        sensitivityLevel,
    };
    static constexpr const char* kServerKeys[] = {
        "FaceDetection.Enable",
        "FaceDetection.SensitivityLevel",
    };
    static constexpr const char* kJsonEventName = "FaceDetection";
    static constexpr const char* kSunapiEventName = "facedetection";

    FaceDetectionGeneral(int /*roiIndex*/ = -1): SettingGroup(kServerKeys) {}
    bool operator==(const FaceDetectionGeneral& rhs) const;
    bool operator!=(const FaceDetectionGeneral& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    // The following functions perhaps should be moved to the inheritor-class.
    // The idea is that the current class interacts with the server only,
    // and the inheritor interacts with the device.
    // The decision will be made during other plugins construction.
    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize /*frameSize*/);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;
};

//-------------------------------------------------------------------------------------------------

// Not implemented because of ambiguities in documentation.
struct FaceDetectionExcludeArea : public SettingGroup
{
    std::vector<PluginPoint> points;

    static constexpr int kStartServerRoiIndexFrom = 1;
    static constexpr int kStartDeviceRoiIndexFrom = 1;

    enum class ServerParamIndex {
        points,
    };
    static constexpr const char* kServerKeys[] = {
        "FaceDetection.DetectionArea#.Points",
    };
    static constexpr const char* kJsonEventName = "FaceDetection";
    static constexpr const char* kSunapiEventName = "facedetection";

    FaceDetectionExcludeArea(int roiIndex = -1) :
        SettingGroup(kServerKeys, roiIndex, kStartServerRoiIndexFrom, kStartDeviceRoiIndexFrom)
    {
    }

    bool operator == (const FaceDetectionExcludeArea& rhs) const;
    bool operator!=(const FaceDetectionExcludeArea& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int roiIndex);
    //void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;
};

//-------------------------------------------------------------------------------------------------
struct ObjectDetectionGeneral: public SettingGroup
{
    bool enabled = false;
    bool person = false;
    bool vehicle = false;
    bool face = false;
    bool licensePlate = false;
    int minimumDuration = 1;

    enum class ServerParamIndex {
        enabled,
        person,
        vehicle,
        face,
        licensePlate,
        minimumDuration,
    };
    static constexpr const char* kServerKeys[] = {
        "ObjectDetection.Enable",
        "ObjectDetection.Person",
        "ObjectDetection.Vehicle",
        "ObjectDetection.Face",
        "ObjectDetection.LicensePlate",
        "ObjectDetection.MinimumDuration",
    };
    static constexpr const char* kJsonEventName = "ObjectDetection";
    static constexpr const char* kSunapiEventName = "objectdetection";

    ObjectDetectionGeneral(int /*roiIndex*/ = -1): SettingGroup(kServerKeys) {}
    bool operator == (const ObjectDetectionGeneral& rhs) const;
    bool operator!=(const ObjectDetectionGeneral& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    //void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;

private:
    std::string buildObjectTypes() const;
};

//-------------------------------------------------------------------------------------------------

struct ObjectDetectionBestShot: public SettingGroup
{
    bool person = false;
    bool vehicle = false;
    bool face = false;
    bool licensePlate = false;

    enum class ServerParamIndex {
        person,
        vehicle,
        face,
        licensePlate,
    };
    static constexpr const char* kServerKeys[] = {
        "ObjectDetection.BestShot.Person",
        "ObjectDetection.BestShot.Vehicle",
        "ObjectDetection.BestShot.Face",
        "ObjectDetection.BestShot.LicensePlate",
    };
    static constexpr const char* kJsonEventName = "ObjectDetection";
    static constexpr const char* kSunapiEventName = "objectdetection";

    ObjectDetectionBestShot(int /*roiIndex*/ = -1): SettingGroup(kServerKeys) {}
    bool operator == (const ObjectDetectionBestShot& rhs) const;
    bool operator!=(const ObjectDetectionBestShot& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    //void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;

private:
    std::string buildObjectTypes() const;
};

//-------------------------------------------------------------------------------------------------

struct ObjectDetectionExcludeArea: public SettingGroup
{
    std::vector<PluginPoint> points;

    static constexpr int kStartServerRoiIndexFrom = 1;
    static constexpr int kStartDeviceRoiIndexFrom = 1;

    enum class ServerParamIndex {
        points,
    };
    static constexpr const char* kServerKeys[] = {
        "ObjectDetection.ExcludeArea#.Points",
    };
    static constexpr const char* kJsonEventName = "ObjectDetection";
    static constexpr const char* kSunapiEventName = "objectdetection";

    ObjectDetectionExcludeArea(int roiIndex = -1):
    SettingGroup(kServerKeys, roiIndex, kStartServerRoiIndexFrom, kStartDeviceRoiIndexFrom)
    {
    }
    bool operator == (const ObjectDetectionExcludeArea& rhs) const;
    bool operator!=(const ObjectDetectionExcludeArea& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int roiIndex);
    //void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;
};

//-------------------------------------------------------------------------------------------------

struct IvaLine: public SettingGroup
{

    std::vector<PluginPoint> points;
    Direction direction = Direction::Right;
    std::string name;
    bool person = false;
    bool vehicle = false;
    bool crossing = false;

    static constexpr int kStartServerRoiIndexFrom = 1;
    static constexpr int kStartDeviceRoiIndexFrom = 1;

    enum class ServerParamIndex {
        points,
        direction,
        name,
        person,
        vehicle,
        crossing,
    };

    static constexpr const char* kServerKeys[] = {
        "IVA.Line#.Points",
        "IVA.Line#.Points", //< direction is also read from "Points"
        "IVA.Line#.Name",
        "IVA.Line#.Person",
        "IVA.Line#.Vehicle",
        "IVA.Line#.Crossing",
    };
    static constexpr const char* kJsonEventName = "VideoAnalysis";
    static constexpr const char* kSunapiEventName = "videoanalysis2";

    IvaLine(int roiIndex = -1):
    SettingGroup(kServerKeys, roiIndex, kStartServerRoiIndexFrom, kStartDeviceRoiIndexFrom)
    {
    }
    bool operator==(const IvaLine& rhs) const;
    bool operator!=(const IvaLine& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int roiIndex);
    //void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;

private:
    std::string buildFilter() const;
    std::string buildMode() const;
};

//-------------------------------------------------------------------------------------------------

struct IvaIncludeArea: public SettingGroup
{
    std::vector<PluginPoint> points;
    std::string name;
    bool person = false;
    bool vehicle = false;

    bool intrusion = false;
    bool enter = false;
    bool exit = false;
    bool appear = false; //< appear/disappear is a single event
    bool loitering = false;

    int intrusionDuration = 0;
    int appearDuration = 10;
    int loiteringDuration = 10;

    static constexpr int kStartServerRoiIndexFrom = 1;
    static constexpr int kStartDeviceRoiIndexFrom = 1;

    enum class ServerParamIndex {
        points,
        name,
        person,
        vehicle,
        intrusion,
        enter,
        exit,
        appear,
        loitering,
        intrusionDuration,
        appearDuration,
        loiteringDuration,
    };
    static constexpr const char* kServerKeys[] = {
        "IVA.IncludeArea#.Points",
        "IVA.IncludeArea#.Name",
        "IVA.IncludeArea#.Person",
        "IVA.IncludeArea#.Vehicle",
        "IVA.IncludeArea#.Intrusion",
        "IVA.IncludeArea#.Enter",
        "IVA.IncludeArea#.Exit",
        "IVA.IncludeArea#.Appear",
        "IVA.IncludeArea#.Loitering",
        "IVA.IncludeArea#.IntrusionDuration",
        "IVA.IncludeArea#.AppearDuration",
        "IVA.IncludeArea#.LoiteringDuration",
    };
    static constexpr const char* kJsonEventName = "VideoAnalysis";
    static constexpr const char* kSunapiEventName = "videoanalysis2";

    IvaIncludeArea(int roiIndex = -1):
    SettingGroup(kServerKeys, roiIndex, kStartServerRoiIndexFrom, kStartDeviceRoiIndexFrom)
    {
    }

    bool operator==(const IvaIncludeArea& rhs) const;
    bool operator!=(const IvaIncludeArea& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int roiIndex);
    //void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;

private:
    std::string buildMode() const;
    std::string buildFilter() const;
};

//-------------------------------------------------------------------------------------------------

struct IvaExcludeArea: public SettingGroup
{
    std::vector<PluginPoint> points;

    static constexpr int kStartServerRoiIndexFrom = 1;
    static constexpr int kStartDeviceRoiIndexFrom = 9;

    enum class ServerParamIndex {
        points,
    };
    static constexpr const char* kServerKeys[] = {
        "IVA.ExcludeArea#.Points",
    };
    static constexpr const char* kJsonEventName = "VideoAnalysis";
    static constexpr const char* kSunapiEventName = "videoanalysis2";

    IvaExcludeArea(int roiIndex = -1):
    SettingGroup(kServerKeys, roiIndex, kStartServerRoiIndexFrom, kStartDeviceRoiIndexFrom)
    {
    }

    bool operator == (const IvaExcludeArea& rhs) const;
    bool operator!=(const IvaExcludeArea& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int roiIndex);
    //void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;
};

//-------------------------------------------------------------------------------------------------

struct AudioDetection: public SettingGroup
{
    bool enabled = false;
    int thresholdLevel = 50;

    enum class ServerParamIndex {
        enabled,
        thresholdLevel,
    };
    static constexpr const char* kServerKeys[] = {
        "AudioDetection.Enable",
        "AudioDetection.ThresholdLevel",
    };
    static constexpr const char* kJsonEventName = "AudioDetection";
    static constexpr const char* kSunapiEventName = "audiodetection";

    AudioDetection(int /*roiIndex*/ = -1): SettingGroup(kServerKeys) {}
    bool operator==(const AudioDetection& rhs) const;
    bool operator!=(const AudioDetection& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;
};

//-------------------------------------------------------------------------------------------------

struct SoundClassification: public SettingGroup
{
    bool enabled = false;
    bool noisefilter = false;
    int thresholdLevel = 50;
    bool scream = false;
    bool gunshot = false;
    bool explosion = false;
    bool crashingGlass = false;

    enum class ServerParamIndex {
        enabled,
        noisefilter,
        thresholdLevel,
        scream,
        gunshot,
        explosion,
        crashingGlass,
    };
    static constexpr const char* kServerKeys[] = {
        "SoundClassification.Enable",
        "SoundClassification.NoiseReduction",
        "SoundClassification.ThresholdLevel",
        "SoundClassification.Scream",
        "SoundClassification.Gunshot",
        "SoundClassification.Explosion",
        "SoundClassification.CrashingGlass",
    };
    static constexpr const char* kJsonEventName = "AudioAnalysis";
    static constexpr const char* kSunapiEventName = "audioanalysis";

    SoundClassification(int /*roiIndex*/ = -1): SettingGroup(kServerKeys) {}
    bool operator==(const SoundClassification& rhs) const;
    bool operator!=(const SoundClassification& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settingsDestination, int /*roiIndex*/) const;

    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;

private:
    std::string buildSoundType() const;
};

//-------------------------------------------------------------------------------------------------
struct ServerValueError {};
struct DeviceValueError {};
//-------------------------------------------------------------------------------------------------
template<class SettingsGroupT>
bool readSettingsFromServer(const nx::sdk::IStringMap* from, SettingsGroupT* to,
    int roiIndex = -1)
{
    SettingsGroupT tmp(roiIndex);
    try
    {
        tmp.readFromServerOrThrow(from, roiIndex);
    }
    catch (ServerValueError&)
    {
        return false;
    }
    *to = std::move(tmp);
    return true;
}
//-------------------------------------------------------------------------------------------------
nx::kit::Json getChannelInfoOrThrow(
    const std::string& cameraReply, const char* eventName, int channelNumber);
//-------------------------------------------------------------------------------------------------
template<class SettingGroupT>
bool readFromDeviceReply(const std::string& from, SettingGroupT* to,
    FrameSize frameSize, int channelNumber, int roiIndex = -1)
{
    SettingGroupT tmp(roiIndex);
    try
    {
        nx::kit::Json channelInfo = getChannelInfoOrThrow(
            from, SettingGroupT::kJsonEventName, channelNumber);

        tmp.readFromCameraOrThrow(channelInfo, frameSize);
    }
    catch (DeviceValueError&)
    {
        return false;
    }
    *to = std::move(tmp);
    return true;
}
//-------------------------------------------------------------------------------------------------
struct Settings
{
    ShockDetection shockDetection;
    Motion motion;
    MotionDetectionObjectSize motionDetectionObjectSize;
    IvaObjectSize ivaObjectSize;
    MotionDetectionIncludeArea motionDetectionIncludeArea[8];
    MotionDetectionExcludeArea motionDetectionExcludeArea[8];
    TamperingDetection tamperingDetection;
    DefocusDetection defocusDetection;
    FogDetection fogDetection;
    FaceDetectionGeneral faceDetectionGeneral;
    //FaceDetectionExcludeArea faceDetectionExcludeArea[8];
    ObjectDetectionGeneral objectDetectionGeneral;
    ObjectDetectionBestShot objectDetectionBestShot;
    ObjectDetectionExcludeArea objectDetectionExcludeArea[8];
    IvaLine ivaLine[8];
    IvaIncludeArea ivaIncludeArea[8];
    IvaExcludeArea ivaExcludeArea[8];
    AudioDetection audioDetection;
    SoundClassification soundClassification;
    SupportedEventCategories supportedCategories = {false};
//    std::array<bool, int(EventCategory::count)> supportedCategories = { false };
};

//-------------------------------------------------------------------------------------------------

// Simple way to check, if T has a member function `empty()`
template <typename T, typename = void>
struct maybeEmpty: std::false_type {};

template <typename T>
struct maybeEmpty<T, std::void_t<decltype(std::declval<T>().points.empty())>>: std::true_type {};

template<class SettingGroupT>
bool differesEnough(const SettingGroupT& lhs, const SettingGroupT& rhs)
{
    if constexpr (maybeEmpty<SettingGroupT>())
    {
        if (lhs.points.empty() && rhs.points.empty())
            return false;
    }

    return lhs != rhs;
}

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
