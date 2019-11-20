#pragma once

#include "point.h"

#include <vector>

#include <nx/kit/json.h>

namespace nx::vms_server_plugins::analytics::hanwha {

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

struct AudioDetection
{
    bool enabled = false;
    int thresholdLevel = 50;
    bool initialized = false;

    static constexpr const char* kPreambule = "msubmenu=audiodetection&action=set";
    static constexpr AnalyticsParam kParams[] = {
        { "AudioDetection.Enable", "&Enable=" },
        { "AudioDetection.ThresholdLevel", "&InputThresholdLevel=" },
    };

    AudioDetection() = default;
    AudioDetection(const std::vector<std::string>& params);
    operator bool() const { return initialized; }
    bool operator==(const AudioDetection& rhs) const;
    bool operator!=(const AudioDetection& rhs) const { return !(*this == rhs); }
};

//-------------------------------------------------------------------------------------------------

/** Current settings engine can not support this setting. */
struct SoundClassification
{
    bool enabled = false;
    bool noisefilter = false;
    int thresholdLevel = 50;
    bool scream = false;
    bool gunshot = false;
    bool explosion = false;
    bool crashingGlass = false;
    bool initialized = false;

    static constexpr const char* kPreambule = "msubmenu=audioanalysis&action=set";
    static constexpr AnalyticsParam kParams[] = {
        { "SoundClassification.Enable", "&Enable=" },
        { "SoundClassification.NoiseReduction", "&NoiseReduction=" },
        { "SoundClassification.ThresholdLevel", "&ThresholdLevel=" },
        { "SoundClassification.Scream", "&SoundType=" },
        { "SoundClassification.Gunshot", "&SoundType=" },
        { "SoundClassification.Explosion", "&SoundType=" },
        { "SoundClassification.CrashingGlass", "&SoundType=" },
    };

    SoundClassification() = default;
    SoundClassification(const std::vector<std::string>& params);
    operator bool() const { return initialized; }
    bool operator==(const SoundClassification& rhs) const;
    bool operator!=(const SoundClassification& rhs) const { return !(*this == rhs); }
};

//-------------------------------------------------------------------------------------------------

struct Settings
{
    ShockDetection shockDetection; //< Analytics::Shock detection

    Motion motion;
    IncludeArea includeArea[8];
    ExcludeArea excludeArea[8];

    TamperingDetection tamperingDetection; //< Analytics:: Tampering detection
    DefocusDetection defocusDetection; //< Analytics::Defocus detection

    AudioDetection audioDetection; //< Analytics::Audio detection
    SoundClassification soundClassification; //< Analytics::Sound classification
};

//-------------------------------------------------------------------------------------------------

// Simple way to check, if T has a member function `empty()`
template <typename T, typename = void>
struct maybeEmpty : std::false_type {};

template <typename T>
struct maybeEmpty<T, std::void_t<decltype(std::declval<T>().empty())>>: std::true_type {};

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
