#pragma once

#include "point.h"
#include "basicServerSettingsIo.h"

#include <array>
#include <vector>
#include <memory>

#include <nx/kit/json.h>

#include <nx/sdk/i_string_map.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/helpers/settings_response.h>

namespace nx::vms_server_plugins::analytics::hanwha {

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

    //template<class E>
    //int k2k(E keyIndexE);

    std::string serverKey(int keyIndex) const;

    template<class E>
    std::string key(E keyIndexE) const;

    template<class E>
    const char* value(const nx::sdk::IStringMap* sourceMap, E keyIndexE);

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

struct AnalyticsMode: public SettingGroup
{
    std::string detectionType;

    enum class KeyIndex {
        detectionType,
    };
    static constexpr const char* kKeys[] = {
        "MotionDetection.DetectionType",
    };
    static constexpr const char* kJsonEventName = "VideoAnalysis";
    static constexpr const char* kSunapiEventName = "videoanalysis2";

    inline static const std::string Off = "Off";
    inline static const std::string MD = "MotionDetection";
    inline static const std::string IV ="IntelligentVideo";
    inline static const std::string MDAndIV ="MDAndIV";

    AnalyticsMode(int /*roiIndex*/ = -1): SettingGroup(kKeys) {}
    bool operator==(const AnalyticsMode& rhs) const;
    bool operator!=(const AnalyticsMode& rhs) const { return !(*this == rhs); }

    [[nodiscard]] AnalyticsMode addIntelligentVideoMode() const;
    [[nodiscard]] AnalyticsMode removeIntelligentVideoMode() const;

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize /*frameSize*/);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;
};

//-------------------------------------------------------------------------------------------------
#if 0
// Hanwha Motion detection support is currently removed from the Clients interface

struct MotionDetectionObjectSize: public SettingGroup
{
    ObjectSizeConstraints constraints;

    enum class KeyIndex {
        constraints,
    };
    static constexpr const char* kKeys[] = {
        "MotionDetection.ObjectSize.Constraints",
    };
    static constexpr const char* kJsonEventName = "VideoAnalysis";
    static constexpr const char* kSunapiEventName = "videoanalysis2";

    MotionDetectionObjectSize(int /*roiIndex*/ = -1): SettingGroup(kKeys) {}
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

struct MotionDetectionIncludeArea: public SettingGroup
{
    UnnamedPolygon unnamedPolygon;
    int thresholdLevel = 50;
    int sensitivityLevel = 80;
    int minimumDuration = 0;

    static constexpr int kStartServerRoiIndexFrom = 1;
    static constexpr int kStartDeviceRoiIndexFrom = 1;

    enum class KeyIndex {
        unnamedPolygon,
        thresholdLevel,
        sensitivityLevel,
        minimumDuration,
    };
    static constexpr const char* kKeys[] = {
        "MotionDetection.IncludeArea#.Points",
        "MotionDetection.IncludeArea#.ThresholdLevel",
        "MotionDetection.IncludeArea#.SensitivityLevel",
        "MotionDetection.IncludeArea#.MinimumDuration",
    };
    static constexpr const char* kJsonEventName = "VideoAnalysis";
    static constexpr const char* kSunapiEventName = "videoanalysis2";

    MotionDetectionIncludeArea(int roiIndex = -1):
    SettingGroup(kKeys, roiIndex, kStartServerRoiIndexFrom, kStartDeviceRoiIndexFrom)
    {
    }
    bool operator==(const MotionDetectionIncludeArea& rhs) const;
    bool operator!=(const MotionDetectionIncludeArea& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int roiIndex);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;
};

//-------------------------------------------------------------------------------------------------

struct MotionDetectionExcludeArea: public SettingGroup
{
    UnnamedPolygon unnamedPolygon;

    static constexpr int kStartServerRoiIndexFrom = 1;
    static constexpr int kStartDeviceRoiIndexFrom = 9;

    enum class KeyIndex {
        unnamedPolygon,
    };
    static constexpr const char* kKeys[] = {
        "MotionDetection.ExcludeArea#.Points",
    };
    static constexpr const char* kJsonEventName = "VideoAnalysis";
    static constexpr const char* kSunapiEventName = "videoanalysis2";

    MotionDetectionExcludeArea(int roiIndex = -1):
    SettingGroup(kKeys, roiIndex, kStartServerRoiIndexFrom, kStartDeviceRoiIndexFrom)
    {
    }

    bool operator == (const MotionDetectionExcludeArea& rhs) const;
    bool operator!=(const MotionDetectionExcludeArea& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int roiIndex);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;
};

#endif
//-------------------------------------------------------------------------------------------------

struct ShockDetection : public SettingGroup
{
    bool enabled = false;
    int thresholdLevel = 50;
    int sensitivityLevel = 80;

    enum class KeyIndex {
        enabled,
        thresholdLevel,
        sensitivityLevel,
    };
    static constexpr const char* kKeys[] = {
        "ShockDetection.Enable",
        "ShockDetection.ThresholdLevel",
        "ShockDetection.SensitivityLevel",
    };
    static constexpr const char* kJsonEventName = "ShockDetection";
    static constexpr const char* kSunapiEventName = "shockdetection";

    ShockDetection(int /*roiIndex*/ = -1) : SettingGroup(kKeys) {}
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

struct TamperingDetection: SettingGroup
{
    bool enabled = false;
    int thresholdLevel = 50;
    int sensitivityLevel = 80;
    int minimumDuration = 0;
    bool exceptDarkImages = false;

    enum class KeyIndex {
        enabled,
        thresholdLevel,
        sensitivityLevel,
        minimumDuration,
        exceptDarkImages,
    };
    static constexpr const char* kKeys[] = {
        "TamperingDetection.Enable",
        "TamperingDetection.ThresholdLevel",
        "TamperingDetection.SensitivityLevel",
        "TamperingDetection.MinimumDuration",
        "TamperingDetection.ExceptDarkImages",
    };
    static constexpr const char* kJsonEventName = "TamperingDetection";
    static constexpr const char* kSunapiEventName = "tamperingdetection";

    TamperingDetection(int /*roiIndex*/ = -1): SettingGroup(kKeys) {}
    bool operator==(const TamperingDetection& rhs) const;
    bool operator!=(const TamperingDetection& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

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

    enum class KeyIndex {
        enabled,
        thresholdLevel,
        sensitivityLevel,
        minimumDuration,
    };
    static constexpr const char* kKeys[] = {
        "DefocusDetection.Enable",
        "DefocusDetection.ThresholdLevel",
        "DefocusDetection.SensitivityLevel",
        "DefocusDetection.MinimumDuration",
    };
    static constexpr const char* kJsonEventName = "DefocusDetection";
    static constexpr const char* kSunapiEventName = "defocusdetection";

    DefocusDetection(int /*roiIndex*/ = -1): SettingGroup(kKeys) {}
    bool operator==(const DefocusDetection& rhs) const;
    bool operator!=(const DefocusDetection& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

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

    enum class KeyIndex {
        enabled,
        thresholdLevel,
        sensitivityLevel,
        minimumDuration,
    };
    static constexpr const char* kKeys[] = {
        "FogDetection.Enable",
        "FogDetection.ThresholdLevel",
        "FogDetection.SensitivityLevel",
        "FogDetection.MinimumDuration",
    };
    static constexpr const char* kJsonEventName = "FogDetection";
    static constexpr const char* kSunapiEventName = "fogdetection";

    FogDetection(int /*roiIndex*/ = -1) : SettingGroup(kKeys) {}
    bool operator==(const FogDetection& rhs) const;
    bool operator!=(const FogDetection& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

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

    enum class KeyIndex {
        enabled,
        person,
        vehicle,
        face,
        licensePlate,
        minimumDuration,
    };
    static constexpr const char* kKeys[] = {
        "ObjectDetection.Enable",
        "ObjectDetection.Person",
        "ObjectDetection.Vehicle",
        "ObjectDetection.Face",
        "ObjectDetection.LicensePlate",
        "ObjectDetection.MinimumDuration",
    };
    static constexpr const char* kJsonEventName = "ObjectDetection";
    static constexpr const char* kSunapiEventName = "objectdetection";

    ObjectDetectionGeneral(int /*roiIndex*/ = -1): SettingGroup(kKeys) {}
    bool operator == (const ObjectDetectionGeneral& rhs) const;
    bool operator!=(const ObjectDetectionGeneral& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

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

    enum class KeyIndex {
        person,
        vehicle,
        face,
        licensePlate,
    };
    static constexpr const char* kKeys[] = {
        "ObjectDetection.BestShot.Person",
        "ObjectDetection.BestShot.Vehicle",
        "ObjectDetection.BestShot.Face",
        "ObjectDetection.BestShot.LicensePlate",
    };
    static constexpr const char* kJsonEventName = "MetaImageTransfer";
    static constexpr const char* kSunapiEventName = "metaimagetransfer";

    ObjectDetectionBestShot(int /*roiIndex*/ = -1): SettingGroup(kKeys) {}
    bool operator == (const ObjectDetectionBestShot& rhs) const;
    bool operator!=(const ObjectDetectionBestShot& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;

private:
    std::string buildObjectTypes() const;
};

//-------------------------------------------------------------------------------------------------

struct IvaObjectSize : public SettingGroup
{
    ObjectSizeConstraints constraints;

    enum class KeyIndex {
        constraints,
    };
    static constexpr const char* kKeys[] = {
        "IVA.ObjectSize.Constraints",
    };
    static constexpr const char* kJsonEventName = "VideoAnalysis";
    static constexpr const char* kSunapiEventName = "videoanalysis2";

    IvaObjectSize(int /*roiIndex*/ = -1) : SettingGroup(kKeys) {}
    bool operator == (const IvaObjectSize& rhs) const;
    bool operator!=(const IvaObjectSize& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;

private:
    std::string buildMinObjectSize(FrameSize frameSize) const;
    std::string buildMaxObjectSize(FrameSize frameSize) const;
};

//-------------------------------------------------------------------------------------------------

struct IvaLine: public SettingGroup
{
    NamedLineFigure namedLineFigure;
    //Direction direction = Direction::Right;
    //std::string name;
    bool person = false;
    bool vehicle = false;
    bool crossing = false;

    static constexpr int kStartServerRoiIndexFrom = 1;
    static constexpr int kStartDeviceRoiIndexFrom = 1;

    enum class KeyIndex {
        namedLineFigure,
        //direction,
        //name,
        person,
        vehicle,
        crossing,
    };

    static constexpr const char* kKeys[] = {
        "IVA.Line#.Points",
        //"IVA.Line#.Points", //< direction is also read from "Points"
        //"IVA.Line#.Name",
        "IVA.Line#.Person",
        "IVA.Line#.Vehicle",
        "IVA.Line#.Crossing",
    };
    static constexpr const char* kJsonEventName = "VideoAnalysis";
    static constexpr const char* kSunapiEventName = "videoanalysis2";

    IvaLine(int roiIndex = -1):
    SettingGroup(kKeys, roiIndex, kStartServerRoiIndexFrom, kStartDeviceRoiIndexFrom)
    {
    }

    bool IsActive() const
    {
        return !namedLineFigure.points.empty() &&  (person || vehicle) && crossing;
    }
    bool operator==(const IvaLine& rhs) const;
    bool operator!=(const IvaLine& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int roiIndex);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;

private:
    std::string buildFilter() const;
    std::string buildMode() const;
};

//-------------------------------------------------------------------------------------------------

struct IvaArea: public SettingGroup
{
    NamedPolygon namedPolygon;
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

    enum class KeyIndex {
        namedPolygon,
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
    static constexpr const char* kKeys[] = {
        "IVA.IncludeArea#.Points",
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

    IvaArea(int roiIndex = -1):
    SettingGroup(kKeys, roiIndex, kStartServerRoiIndexFrom, kStartDeviceRoiIndexFrom)
    {
    }
    bool IsActive() const
    {
        return !namedPolygon.points.empty()
            && (person || vehicle) && (intrusion || enter || exit || appear || loitering);
    }
    bool operator==(const IvaArea& rhs) const;
    bool operator!=(const IvaArea& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int roiIndex);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;

private:
    std::string buildMode() const;
    std::string buildFilter() const;
};

//-------------------------------------------------------------------------------------------------

struct IvaExcludeArea: public SettingGroup
{
    UnnamedPolygon unnamedPolygon;

    static constexpr int kStartServerRoiIndexFrom = 1;
    static constexpr int kStartDeviceRoiIndexFrom = 9;

    enum class KeyIndex {
        unnamedPolygon,
    };
    static constexpr const char* kKeys[] = {
        "IVA.ExcludeArea#.Points",
    };
    static constexpr const char* kJsonEventName = "VideoAnalysis";
    static constexpr const char* kSunapiEventName = "videoanalysis2";

    IvaExcludeArea(int roiIndex = -1):
    SettingGroup(kKeys, roiIndex, kStartServerRoiIndexFrom, kStartDeviceRoiIndexFrom)
    {
    }

    bool operator == (const IvaExcludeArea& rhs) const;
    bool operator!=(const IvaExcludeArea& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int roiIndex);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;
};

//-------------------------------------------------------------------------------------------------

struct AudioDetection: public SettingGroup
{
    bool enabled = false;
    int thresholdLevel = 50;

    enum class KeyIndex {
        enabled,
        thresholdLevel,
    };
    static constexpr const char* kKeys[] = {
        "AudioDetection.Enable",
        "AudioDetection.ThresholdLevel",
    };
    static constexpr const char* kJsonEventName = "AudioDetection";
    static constexpr const char* kSunapiEventName = "audiodetection";

    AudioDetection(int /*roiIndex*/ = -1): SettingGroup(kKeys) {}
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

    enum class KeyIndex {
        enabled,
        noisefilter,
        thresholdLevel,
        scream,
        gunshot,
        explosion,
        crashingGlass,
    };
    static constexpr const char* kKeys[] = {
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

    SoundClassification(int /*roiIndex*/ = -1): SettingGroup(kKeys) {}
    bool operator==(const SoundClassification& rhs) const;
    bool operator!=(const SoundClassification& rhs) const { return !(*this == rhs); }

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settingsDestination, int /*roiIndex*/ = -1) const;

    void readFromCameraOrThrow(const nx::kit::Json& channelInfo, FrameSize frameSize);
    std::string buildCameraWritingQuery(FrameSize /*frameSize*/, int channelNumber) const;

private:
    std::string buildSoundType() const;
};

//-------------------------------------------------------------------------------------------------
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

} // namespace nx::vms_server_plugins::analytics::hanwha
