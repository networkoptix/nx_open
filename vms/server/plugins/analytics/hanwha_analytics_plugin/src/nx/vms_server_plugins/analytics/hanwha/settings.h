#pragma once

#include "point.h"

#include <vector>

#include <nx/kit/json.h>

#include <nx/sdk/i_string_map.h>
#include <nx/sdk/helpers/string_map.h>

namespace nx::vms_server_plugins::analytics::hanwha {

//-------------------------------------------------------------------------------------------------

const char* upperIfBoolean(const char* value);

std::string specify(const char* v, unsigned int n);

//-------------------------------------------------------------------------------------------------

struct AnalyticsParam
{
    const char* plugin;
    const char* sunapi;
};

/** temporary crutch till C++20 std::span */
class AnalyticsParamSpan
{
public:
    template<int N>
    AnalyticsParamSpan(const AnalyticsParam(&params)[N]) noexcept:
        m_begin(params),
        m_end(params + std::size(params))
    {
    }
    const AnalyticsParam* begin() const { return m_begin; }
    const AnalyticsParam* end() const { return m_end; }
    size_t size() const {return m_end - m_begin; }

private:
    const AnalyticsParam* m_begin;
    const AnalyticsParam* m_end;
};

//-------------------------------------------------------------------------------------------------

// 1. ShockDetection. SUNAPI 2.5.4 (2018-08-07) 2.23
struct ShockDetection
{
    bool enabled = false;
    int thresholdLevel = 50;
    int sensitivityLevel = 80;
    bool initialized = false;

    static constexpr const char* kPreambule = "msubmenu=shockdetection&action=set";
    static constexpr AnalyticsParam kParams[] = {
        { "ShockDetection.Enable", "&Enable=" },
        { "ShockDetection.ThresholdLevel", "&ThresholdLevel=" },
        { "ShockDetection.SensitivityLevel", "&Sensitivity=" },
    };

    ShockDetection() = default;
    ShockDetection(const std::vector<std::string>& params);
    operator bool() const { return initialized; }
    bool operator==(const ShockDetection& rhs) const;
    bool operator!=(const ShockDetection& rhs) const { return !(*this == rhs); }
};

//-------------------------------------------------------------------------------------------------

struct Motion
{
    std::string detectionType;
    bool initialized = false;

    static constexpr const char* kPreambule =
        "msubmenu=videoanalysis2&action=set";//&DetectionType=MotionDetection";

    static constexpr AnalyticsParam kParams[] = {
        { "MotionDetection.DetectionType", "&DetectionType=" },
    };

    Motion() = default;
    Motion(const std::vector<std::string>& params);
    operator bool() const { return initialized; }
    bool operator==(const Motion& rhs) const;
    bool operator!=(const Motion& rhs) const { return !(*this == rhs); }
};

//-------------------------------------------------------------------------------------------------

struct IncludeArea
{
    std::vector<SunapiPoint> points;
    int thresholdLevel = 50;
    int sensitivityLevel = 80;
    int minimumDuration = 0;
    bool initialized = false;

    static constexpr const char* kPreambule =
        "msubmenu=videoanalysis2&action=set";//&DetectionType=MotionDetection";
    static constexpr AnalyticsParam kParams[] = {
        //{ "MotionDetection.Enable", "&Enable=" }, // No `Enable` for Motion in SUNAPI.
        { "MotionDetection.IncludeArea#.Points", "&ROI.#.Coordinate=" },
        { "MotionDetection.IncludeArea#.ThresholdLevel", "&ROI.#.ThresholdLevel=" },
        { "MotionDetection.IncludeArea#.SensitivityLevel", "&ROI.#.SensitivityLevel=" },
        { "MotionDetection.IncludeArea#.MinimumDuration", "&ROI.#.Duration=" },
    };
    static constexpr const char* kAlternativeCommand =
        "msubmenu=videoanalysis2&action=remove&ROIIndex=#";

    IncludeArea() = default;
    IncludeArea(const std::vector<std::string>& params);
    operator bool() const { return initialized; }
    bool empty() const { return points.empty(); }
    bool operator==(const IncludeArea& rhs) const;
    bool operator!=(const IncludeArea& rhs) const { return !(*this == rhs); }
};

//-------------------------------------------------------------------------------------------------

struct ExcludeArea
{
    std::vector<SunapiPoint> points;
    bool initialized = false;
    static constexpr const char* kPreambule =
        "msubmenu=videoanalysis2&action=set";//&DetectionType=MotionDetection";
    static constexpr AnalyticsParam kParams[] = {
        //{ "MotionDetection.Enable", "&Enable=" }, // No `Enable` for Motion in SUNAPI.
        { "MotionDetection.ExcludeArea#.Points", "&ROI.#.Coordinate=" },
    };
    static constexpr const char* kAlternativeCommand =
        "msubmenu=videoanalysis2&action=remove&ROIIndex=#";

    ExcludeArea() = default;
    ExcludeArea(const std::vector<std::string>& params);
    operator bool() const { return initialized; }
    bool empty() const { return points.empty(); }
    bool operator==(const ExcludeArea& rhs) const { return points == rhs.points; }
    bool operator!=(const ExcludeArea& rhs) const { return !(*this == rhs); }
};

//-------------------------------------------------------------------------------------------------

struct TamperingDetection
{
    bool enabled = false;
    int thresholdLevel = 50;
    int sensitivityLevel = 80;
    int minimumDuration = 0;
    bool exceptDarkImages = false;
    bool initialized = false;

    static constexpr const char* kPreambule = "msubmenu=tamperingdetection&action=set";
    static constexpr AnalyticsParam kParams[] = {
        { "TamperingDetection.Enable", "&Enable=" },
        { "TamperingDetection.ThresholdLevel", "&ThresholdLevel=" },
        { "TamperingDetection.SensitivityLevel", "&SensitivityLevel=" },
        { "TamperingDetection.MinimumDuration", "&Duration=" },
        { "TamperingDetection.ExceptDarkImages", "&DarknessDetection=" },
    };

    TamperingDetection() = default;
    TamperingDetection(const std::vector<std::string>& params);
    operator bool() const { return initialized; }
    bool operator==(const TamperingDetection& rhs) const;
    bool operator!=(const TamperingDetection& rhs) const { return !(*this == rhs); }
};

//-------------------------------------------------------------------------------------------------

struct DefocusDetection
{
    bool enabled = false;
    int thresholdLevel = 50;
    int sensitivityLevel = 80;
    int minimumDuration = 0;
    bool initialized = false;

    static constexpr const char* kPreambule = "msubmenu=defocusdetection&action=set";
    static constexpr AnalyticsParam kParams[] = {
        { "DefocusDetection.Enable", "&Enable=" },
        { "DefocusDetection.ThresholdLevel", "&ThresholdLevel=" },
        { "DefocusDetection.SensitivityLevel", "&Sensitivity=" },
        { "DefocusDetection.MinimumDuration", "&Duration=" },
    };

    DefocusDetection() = default;
    DefocusDetection(const std::vector<std::string>& params);
    operator bool() const { return initialized; }
    bool operator==(const DefocusDetection& rhs) const;
    bool operator!=(const DefocusDetection& rhs) const { return !(*this == rhs); }
};

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
// New Settings Engine - for non-symmetric settings.
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

struct SettingGroup
{
    bool initialized = false;
    operator bool() const { return initialized; }
};

//-------------------------------------------------------------------------------------------------

struct ObjectDetectionObjects : public SettingGroup
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
    static constexpr const char* kServerParamsNames[] = {
        "ObjectDetection.Enable",
        "ObjectDetection.Person",
        "ObjectDetection.Vehicle",
        "ObjectDetection.Face",
        "ObjectDetection.LicensePlate",
        "ObjectDetection.MinimumDuration",
    };

    ObjectDetectionObjects() = default;
    bool operator == (const ObjectDetectionObjects& rhs) const;
    bool operator!=(const ObjectDetectionObjects& rhs) const { return !(*this == rhs); }
    bool differesEnoughFrom(const ObjectDetectionObjects& rhs) const { return !(*this == rhs); }
    bool loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex = 0);
    void replanishErrorMap(
        nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const;
};

std::string buildCameraRequestQuery(
    const ObjectDetectionObjects& area, FrameSize frameSize, int channelNumber = 0);

//-------------------------------------------------------------------------------------------------

struct ObjectDetectionBestShot : public SettingGroup
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
    static constexpr const char* kServerParamsNames[] = {
        "ObjectDetection.BestShot.Person",
        "ObjectDetection.BestShot.Vehicle",
        "ObjectDetection.BestShot.Face",
        "ObjectDetection.BestShot.LicensePlate",
    };

    ObjectDetectionBestShot() = default;
    bool operator == (const ObjectDetectionBestShot& rhs) const;
    bool operator!=(const ObjectDetectionBestShot& rhs) const { return !(*this == rhs); }
    bool differesEnoughFrom(const ObjectDetectionBestShot& rhs) const { return !(*this == rhs); }
    bool loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex = 0);
    void replanishErrorMap(
        nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const;
};

std::string buildCameraRequestQuery(
    const ObjectDetectionBestShot& area, FrameSize frameSize, int channelNumber = 0);

//-------------------------------------------------------------------------------------------------

struct ObjectDetectionExcludeArea : public SettingGroup
{
    std::vector<PluginPoint> points;

    int internalObjectIndex = 0;

    enum class ServerParamIndex {
        points,
    };
    static constexpr const char* kServerParamsNames[] = {
        "ObjectDetection.ExcludeArea#.Points",
    };

    ObjectDetectionExcludeArea() = default;
    bool operator == (const ObjectDetectionExcludeArea& rhs) const;
    bool operator!=(const ObjectDetectionExcludeArea& rhs) const { return !(*this == rhs); }
    bool differesEnoughFrom(const ObjectDetectionExcludeArea& rhs) const;
    bool loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex = 0);
    void replanishErrorMap(
        nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const;
};

std::string buildCameraRequestQuery(
    const ObjectDetectionExcludeArea& area, FrameSize frameSize, int channelNumber = 0);

//-------------------------------------------------------------------------------------------------

struct IvaLine : public SettingGroup
{

    std::vector<PluginPoint> points;
    Direction direction = Direction::Right;
    std::string name;
    bool person = false;
    bool vehicle = false;
    bool crossing = false;

    int internalObjectIndex = 0;

    enum class ServerParamIndex {
        points,
        direction,
        name,
        person,
        vehicle,
        crossing,
    };
    static constexpr const char* kServerParamsNames[] = {
        "IVA.Line#.Points",
        "IVA.Line#.Points", //< direction is also read from "Points"
        "IVA.Line#.Name",
        "IVA.Line#.Person",
        "IVA.Line#.Vehicle",
        "IVA.Line#.Crossing",
    };

    IvaLine() = default;

    bool operator==(const IvaLine& rhs) const;
    bool operator!=(const IvaLine& rhs) const { return !(*this == rhs); }
    bool differesEnoughFrom(const IvaLine& rhs) const;
    bool loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex = 0);
    void replanishErrorMap(
        nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const;

};

std::string buildCameraRequestQuery(
    const IvaLine& area, FrameSize frameSize, int channelNumber = 0);

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

    // For Include areas internal and external (gui) indices are the same.
    int internalObjectIndex = 0;

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
    static constexpr const char* kServerParamsNames[] = {
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

    IvaIncludeArea() = default;

    bool operator==(const IvaIncludeArea& rhs) const;
    bool operator!=(const IvaIncludeArea& rhs) const { return !(*this == rhs); }
    bool differesEnoughFrom(const IvaIncludeArea& rhs) const;
    bool loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex = 0);
    void replanishErrorMap(
        nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const;
};
std::string buildCameraRequestQuery(
    const IvaIncludeArea& area, FrameSize frameSize, int channelNumber = 0);

//-------------------------------------------------------------------------------------------------

struct IvaExcludeArea: public SettingGroup
{
    std::vector<PluginPoint> points;

    // For Exclude areas internal index = external (gui) indix + 8.
    int internalObjectIndex = 0;

    enum class ServerParamIndex {
        points,
    };
    static constexpr const char* kServerParamsNames[] = {
        "IVA.ExcludeArea#.Points",
    };

    IvaExcludeArea() = default;
    bool operator == (const IvaExcludeArea& rhs) const;
    bool operator!=(const IvaExcludeArea& rhs) const { return !(*this == rhs); }
    bool differesEnoughFrom(const IvaExcludeArea& rhs) const;
    bool loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex = 0);
    void replanishErrorMap(
        nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const;
};

std::string buildCameraRequestQuery(
    const IvaExcludeArea& area, FrameSize frameSize, int channelNumber = 0);

//-------------------------------------------------------------------------------------------------

struct AudioDetection : public SettingGroup
{
    bool enabled = false;
    int thresholdLevel = 50;

    enum class ServerParamIndex {
        enabled,
        thresholdLevel,
    };
    static constexpr const char* kServerParamsNames[] = {
        "AudioDetection.Enable",
        "AudioDetection.ThresholdLevel",
    };

    AudioDetection() = default;
    bool operator==(const AudioDetection& rhs) const;
    bool operator!=(const AudioDetection& rhs) const { return !(*this == rhs); }
    bool differesEnoughFrom(const AudioDetection& rhs) const { return !(*this == rhs); }
    bool loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex = 0);
    void replanishErrorMap(
        nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const;
};
std::string buildCameraRequestQuery(
    const AudioDetection& area, FrameSize /*frameSize*/, int channelNumber = 0);

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
    static constexpr const char* kServerParamsNames[] = {
        "SoundClassification.Enable",
        "SoundClassification.NoiseReduction",
        "SoundClassification.ThresholdLevel",
        "SoundClassification.Scream",
        "SoundClassification.Gunshot",
        "SoundClassification.Explosion",
        "SoundClassification.CrashingGlass",
    };

    SoundClassification() = default;
    bool operator==(const SoundClassification& rhs) const;
    bool operator!=(const SoundClassification& rhs) const { return !(*this == rhs); }
    bool differesEnoughFrom(const SoundClassification& rhs) const { return !(*this == rhs); }
    bool loadFromServer(const nx::sdk::IStringMap* settings, int objectIndex = 0);
    void replanishErrorMap(
        nx::sdk::Ptr<nx::sdk::StringMap>& errorMap, const std::string& reason) const;
};

std::string buildCameraRequestQuery(
    const SoundClassification& area, FrameSize /*frameSize*/, int channelNumber = 0);

//-------------------------------------------------------------------------------------------------

struct Settings
{
    ShockDetection shockDetection; //< Analytics::Shock detection

    Motion motion;
    IncludeArea includeArea[8];
    ExcludeArea excludeArea[8];

    TamperingDetection tamperingDetection; //< Analytics:: Tampering detection
    DefocusDetection defocusDetection; //< Analytics::Defocus detection

    ObjectDetectionObjects objectDetectionObjects;
    ObjectDetectionBestShot objectDetectionBestShot;
    ObjectDetectionExcludeArea objectDetectionExcludeArea[8];
    IvaLine ivaLine[8];
    IvaIncludeArea ivaIncludeArea[8];
    IvaExcludeArea ivaExcludeArea[8];
    AudioDetection audioDetection;
    SoundClassification soundClassification;
};

//-------------------------------------------------------------------------------------------------

// Simple way to check, if T has a member function `empty()`
template <typename T, typename = void>
struct maybeEmpty : std::false_type {};

template <typename T>
struct maybeEmpty<T, std::void_t<decltype(std::declval<T>().empty())>>: std::true_type {};

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
