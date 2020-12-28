#pragma once

#include <array>
#include <vector>
#include <memory>

#include <nx/kit/json.h>

#include <nx/sdk/i_string_map.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/helpers/settings_response.h>

#include <nx/utils/log/log.h>
#include <nx/utils/log/log_message.h>

#include "setting_primitives.h"
#include "setting_primitives_io.h"
#include "device_response_json_parser.h" //< low level parsing
#include "settings_capabilities.h"

namespace nx::vms_server_plugins::analytics::hanwha {

//-------------------------------------------------------------------------------------------------

/**
 * SettingsGroup is a base class for any group of ROI settings. Each group contains one or several
 * settings that correspond to an event. Each member-variable describes one setting. Members'
 * default values correspond to settings default values (values after device reset).
 */
struct SettingGroup
{
    const SettingsCapabilities& m_settingsCapabilities; //< settingsCapabilities are stored in the DeviceAgent
    const RoiResolution& m_roiResolution; //< roiResolution is stored in the DeviceAgent
    // Server contains a map, where the values of all settings are stored. The keys to values, that
    // correspond to the current group of settings, are stored in the string array `serverKeys`.
    // Both `serverKeys` and `serverKeyCount` are initialized by descendants of this class.
    const char* const* const serverKeys;
    const int serverKeyCount;

    template<int N>
    SettingGroup(
        const SettingsCapabilities& settingsCapabilities,
        const RoiResolution& roiResolution,
        const char* const(&paramsArray)[N],
        int nativeIndex = -1,
        int serverIndexOffset = 0,
        int deviceIndexOffset = 0)
    :
        m_settingsCapabilities(settingsCapabilities),
        m_roiResolution(roiResolution),
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

    template<class E>
    std::string key(E keyIndexE) const;

    template<class E>
    const char* value(const nx::sdk::IStringMap* sourceMap, E keyIndexE);

    void replenishErrorMap(nx::sdk::StringMap* errorMap, const std::string& reason) const;
    void replenishValueMap(
        nx::sdk::StringMap* valueMap, const nx::sdk::IStringMap* sourceMap) const;

    int nativeIndex() const { return m_nativeIndex; }
    int serverIndex() const { return m_serverIndex; }
    int deviceIndex() const { return m_deviceIndex; }

    template<class SettingGroupT>
    static bool readFromServer(const nx::sdk::IStringMap* from, SettingGroupT* to,
        int roiIndex = -1)
    {
        SettingGroupT tmp(to->m_settingsCapabilities, to->m_roiResolution, roiIndex);
        try
        {
            tmp.readFromServerOrThrow(from, roiIndex);
        }
        catch (const SettingPrimitivesServerIo::DeserializationError&)
        {
            return false;
        }
        *to = std::move(tmp);
        return true;
    }

    template<class SettingGroupT>
    static bool readFromDeviceReply(const std::string& from, SettingGroupT* to,
        int channelNumber, int roiIndex = -1)
    {
        SettingGroupT tmp(to->m_settingsCapabilities, to->m_roiResolution, roiIndex);
        try
        {
            nx::kit::Json channelInfo = DeviceResponseJsonParser::extractChannelInfo(
                from, SettingGroupT::kJsonEventName, channelNumber);

            tmp.readFromDeviceReplyOrThrow(channelInfo);
        }
        catch (const SettingPrimitivesDeviceIo::CameraResponseJsonError&)
        {
            return false;
        }
        tmp.assignExclusiveFrom(*to);
        *to = std::move(tmp);
        return true;
    }

    /**
     * The core function - transfers settings from server to device.
     * Read a bunch of settings `settingGroup` from the server (from `sourceMap`) and write it
     * to the agent's device, using `sendingFunction`.
     */
    template<class SettingGroupT, class SendingFunctor>
    static void transferFromServerToDevice(
        nx::sdk::StringMap* errorMap,
        nx::sdk::StringMap* valueMap,
        const nx::sdk::IStringMap* sourceMap,
        SettingGroupT& previousState,
        SendingFunctor sendingFunctor,
        int channelNumber,
        int objectIndex = -1)
    {
        SettingGroupT settingGroup(previousState.m_settingsCapabilities, previousState.m_roiResolution);
        if (!SettingGroup::readFromServer(sourceMap, &settingGroup, objectIndex))
        {
            settingGroup.replenishErrorMap(errorMap, "read failed");
            return;
        }

        if (settingGroup == previousState)
            return;

        const std::string settingQuery =
            settingGroup.buildDeviceWritingQuery(channelNumber);

        const std::string error = sendingFunctor(settingQuery);

        NX_DEBUG(typeid(SettingGroupT), NX_FMT(
            "Hanwha plugin sent a settings request to Hanwha camera: request = %1, resulting error = %2",
            settingQuery, error));

        if (!error.empty())
        {
            settingGroup.replenishErrorMap(errorMap, error);
        }
        else
        {
            #if 0
                // While plugin manager is under construction its not clear if it is needed.
                settingGroup.replenishValueMap(valueMap, sourceMap);
            #endif
            previousState = settingGroup;
        }
    }

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

    AnalyticsMode(const SettingsCapabilities& settingsCapabilities,
        const RoiResolution& roiResolution, int /*roiIndex*/ = -1)
        :
        SettingGroup(settingsCapabilities, roiResolution, kKeys)
    {
    }
    bool operator==(const AnalyticsMode& rhs) const;
    bool operator!=(const AnalyticsMode& rhs) const { return !(*this == rhs); }

    void assignExclusiveFrom(const AnalyticsMode& other) {};

    [[nodiscard]] AnalyticsMode addIntelligentVideoMode() const;
    [[nodiscard]] AnalyticsMode removeIntelligentVideoMode() const;

    void readExclusiveFromServer(const nx::sdk::IStringMap* settings) {}
    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo);
    std::string buildDeviceWritingQuery(int channelNumber) const;
};

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

    ShockDetection(const SettingsCapabilities& settingsCapabilities,
        const RoiResolution& roiResolution, int /*roiIndex*/ = -1)
        :
        SettingGroup(settingsCapabilities, roiResolution, kKeys)
    {
    }
    bool operator==(const ShockDetection& rhs) const;
    bool operator!=(const ShockDetection& rhs) const { return !(*this == rhs); }
    void assignExclusiveFrom(const ShockDetection& other) {};

    void readExclusiveFromServer(const nx::sdk::IStringMap* settings) {}
    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    // The following functions perhaps should be moved to the inheritor-class.
    // The idea is that the current class interacts with the server only,
    // and the inheritor interacts with the device.
    // The decision will be made during other plugins construction.
    void readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo);
    std::string buildDeviceWritingQuery(int channelNumber) const;
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

    TamperingDetection(const SettingsCapabilities& settingsCapabilities,
        const RoiResolution& roiResolution, int /*roiIndex*/ = -1)
        :
        SettingGroup(settingsCapabilities, roiResolution, kKeys)
    {
    }
    bool operator==(const TamperingDetection& rhs) const;
    bool operator!=(const TamperingDetection& rhs) const { return !(*this == rhs); }
    void assignExclusiveFrom(const TamperingDetection& other) {};

    void readExclusiveFromServer(const nx::sdk::IStringMap* settings) {}
    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo);
    std::string buildDeviceWritingQuery(int channelNumber) const;
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

    DefocusDetection(const SettingsCapabilities& settingsCapabilities, const RoiResolution& roiResolution, int /*roiIndex*/ = -1):
        SettingGroup(settingsCapabilities, roiResolution, kKeys) {}
    bool operator==(const DefocusDetection& rhs) const;
    bool operator!=(const DefocusDetection& rhs) const { return !(*this == rhs); }
    void assignExclusiveFrom(const DefocusDetection& other) {};

    void readExclusiveFromServer(const nx::sdk::IStringMap* settings) {}
    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo);
    std::string buildDeviceWritingQuery(int channelNumber) const;
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

    FogDetection(const SettingsCapabilities& settingsCapabilities,
        const RoiResolution& roiResolution, int /*roiIndex*/ = -1)
        :
        SettingGroup(settingsCapabilities, roiResolution, kKeys)
    {
    }
    bool operator==(const FogDetection& rhs) const;
    bool operator!=(const FogDetection& rhs) const { return !(*this == rhs); }
    void assignExclusiveFrom(const FogDetection& other) {};

    void readExclusiveFromServer(const nx::sdk::IStringMap* settings) {}
    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo);
    std::string buildDeviceWritingQuery(int channelNumber) const;
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

    ObjectDetectionGeneral(const SettingsCapabilities& settingsCapabilities,
        const RoiResolution& roiResolution, int /*roiIndex*/ = -1)
        :
        SettingGroup(settingsCapabilities, roiResolution, kKeys)
    {
    }
    bool operator == (const ObjectDetectionGeneral& rhs) const;
    bool operator!=(const ObjectDetectionGeneral& rhs) const { return !(*this == rhs); }
    void assignExclusiveFrom(const ObjectDetectionGeneral& other) {};

    void readExclusiveFromServer(const nx::sdk::IStringMap* settings) {}
    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo);
    std::string buildDeviceWritingQuery(int channelNumber) const;

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

    ObjectDetectionBestShot(const SettingsCapabilities& settingsCapabilities,
        const RoiResolution& roiResolution, int /*roiIndex*/ = -1)
        :
        SettingGroup(settingsCapabilities, roiResolution, kKeys)
    {
    }
    bool operator == (const ObjectDetectionBestShot& rhs) const;
    bool operator!=(const ObjectDetectionBestShot& rhs) const { return !(*this == rhs); }
    void assignExclusiveFrom(const ObjectDetectionBestShot& other) {};

    void readExclusiveFromServer(const nx::sdk::IStringMap* settings) {}
    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo);
    std::string buildDeviceWritingQuery(int channelNumber) const;

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

    IvaObjectSize(const SettingsCapabilities& settingsCapabilities,
        const RoiResolution& roiResolution, int /*roiIndex*/ = -1)
        :
        SettingGroup(settingsCapabilities, roiResolution, kKeys)
    {
    }
    bool operator == (const IvaObjectSize& rhs) const;
    bool operator!=(const IvaObjectSize& rhs) const { return !(*this == rhs); }
    void assignExclusiveFrom(const IvaObjectSize& other) {};

    void readExclusiveFromServer(const nx::sdk::IStringMap* settings) {}
    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo);
    std::string buildDeviceWritingQuery(int channelNumber) const;

private:
    std::string buildMinObjectSize() const;
    std::string buildMaxObjectSize() const;
};

//-------------------------------------------------------------------------------------------------

struct IvaLine: public SettingGroup
{
    NamedLineFigure namedLineFigure;
    bool person = false;
    bool vehicle = false;
    bool crossing = false;

    static constexpr int kStartServerRoiIndexFrom = 1;
    static constexpr int kStartDeviceRoiIndexFrom = 1;

    enum class KeyIndex {
        namedLineFigure,
        person,
        vehicle,
        crossing,
    };

    static constexpr const char* kKeys[] = {
        "IVA.Line#.Points",
        "IVA.Line#.Person",
        "IVA.Line#.Vehicle",
        "IVA.Line#.Crossing",
    };
    static constexpr const char* kJsonEventName = "VideoAnalysis";
    static constexpr const char* kSunapiEventName = "videoanalysis2";

    IvaLine(const SettingsCapabilities& settingsCapabilities, const RoiResolution& roiResolution,
        int roiIndex = -1)
        :
        SettingGroup(settingsCapabilities, roiResolution, kKeys, roiIndex,
            kStartServerRoiIndexFrom, kStartDeviceRoiIndexFrom)
    {
    }

    bool IsActive() const
    {
        if (m_settingsCapabilities.ivaLine.objectTypeFilter)
            return !namedLineFigure.points.empty() &&  (person || vehicle) && crossing;
        else
            return !namedLineFigure.points.empty() && crossing;
    }
    bool operator==(const IvaLine& rhs) const;
    bool operator!=(const IvaLine& rhs) const { return !(*this == rhs); }
    void assignExclusiveFrom(const IvaLine& other);

    void readExclusiveFromServer(const nx::sdk::IStringMap* settings);
    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int roiIndex);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo);
    std::string buildDeviceWritingQuery(int channelNumber) const;

private:
    static Direction invertedDirection(Direction direction);
    std::string buildFilter() const;
    std::string buildMode(/*bool inverted = false*/) const;
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

    IvaArea(const SettingsCapabilities& settingsCapabilities, const RoiResolution& roiResolution,
        int roiIndex = -1)
        :
        SettingGroup(settingsCapabilities, roiResolution, kKeys, roiIndex,
            kStartServerRoiIndexFrom, kStartDeviceRoiIndexFrom)
    {
    }
    bool IsActive() const
    {
        const bool isAnyObjectTypeOn = m_settingsCapabilities.ivaArea.objectTypeFilter ? (person || vehicle) : true;
        const bool isAnyEventTypeOn = m_settingsCapabilities.ivaArea.intrusion && intrusion
            || m_settingsCapabilities.ivaArea.enter && enter
            || m_settingsCapabilities.ivaArea.exit && exit
            || m_settingsCapabilities.ivaArea.appear && appear
            || m_settingsCapabilities.ivaArea.loitering && loitering;
        return !namedPolygon.points.empty() && isAnyObjectTypeOn && isAnyEventTypeOn;
    }
    bool operator==(const IvaArea& rhs) const;
    bool operator!=(const IvaArea& rhs) const { return !(*this == rhs); }
    void assignExclusiveFrom(const IvaArea& other);

    void readExclusiveFromServer(const nx::sdk::IStringMap* settings);
    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int roiIndex);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo);
    std::string buildDeviceWritingQuery(int channelNumber) const;

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

    IvaExcludeArea(const SettingsCapabilities& settingsCapabilities,
        const RoiResolution& roiResolution, int roiIndex = -1)
        :
        SettingGroup(settingsCapabilities, roiResolution, kKeys, roiIndex,
            kStartServerRoiIndexFrom, kStartDeviceRoiIndexFrom)
    {
    }

    bool operator == (const IvaExcludeArea& rhs) const;
    bool operator!=(const IvaExcludeArea& rhs) const { return !(*this == rhs); }
    void assignExclusiveFrom(const IvaExcludeArea& other) {}

    void readExclusiveFromServer(const nx::sdk::IStringMap* settings) {}
    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int roiIndex);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo);
    std::string buildDeviceWritingQuery(int channelNumber) const;
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

    AudioDetection(const SettingsCapabilities& settingsCapabilities,
        const RoiResolution& roiResolution, int /*roiIndex*/ = -1)
        :
        SettingGroup(settingsCapabilities, roiResolution, kKeys)
    {
    }
    bool operator==(const AudioDetection& rhs) const;
    bool operator!=(const AudioDetection& rhs) const { return !(*this == rhs); }
    void assignExclusiveFrom(const AudioDetection& other) {}

    void readExclusiveFromServer(const nx::sdk::IStringMap* settings) {}
    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo);
    std::string buildDeviceWritingQuery(int channelNumber) const;
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

    SoundClassification(const SettingsCapabilities& settingsCapabilities,
        const RoiResolution& roiResolution, int /*roiIndex*/ = -1)
        :
        SettingGroup(settingsCapabilities, roiResolution, kKeys)
    {
    }
    bool operator==(const SoundClassification& rhs) const;
    bool operator!=(const SoundClassification& rhs) const { return !(*this == rhs); }
    void assignExclusiveFrom(const SoundClassification& other) {}

    void readExclusiveFromServer(const nx::sdk::IStringMap* settings) {}

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settingsDestination, int /*roiIndex*/ = -1) const;

    void readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo);
    std::string buildDeviceWritingQuery(int channelNumber) const;

private:
    std::string buildSoundType() const;
};

//-------------------------------------------------------------------------------------------------

struct FaceMaskDetection: public SettingGroup
{
    enum class DetectionMode
    {
        noMask,
        mask,
    };

    bool enabled = false;
    DetectionMode detectionMode = DetectionMode::noMask;
    int duration = 1;

    enum class KeyIndex {
        enabled,
        detectionMode,
        duration,
    };
    static constexpr const char* kKeys[] = {
        "FaceMaskDetection.Enable",
        "FaceMaskDetection.DetectionMode",
        "FaceMaskDetection.Duration",
    };
    static constexpr const char* kJsonEventName = "MaskDetection";
    static constexpr const char* kSunapiEventName = "maskdetection";

    FaceMaskDetection(const SettingsCapabilities& settingsCapabilities,
        const RoiResolution& roiResolution, int /*roiIndex*/ = -1)
        :
        SettingGroup(settingsCapabilities, roiResolution, kKeys)
    {
    }
    bool operator==(const FaceMaskDetection& rhs) const;
    bool operator!=(const FaceMaskDetection& rhs) const { return !(*this == rhs); }
    void assignExclusiveFrom(const FaceMaskDetection& other) {}

    void readExclusiveFromServer(const nx::sdk::IStringMap* settings) {}
    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settingsDestination, int /*roiIndex*/ = -1) const;

    void readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo);
    std::string buildDeviceWritingQuery(int channelNumber) const;

private:
    static constexpr const char* kMaskDetectionMode = "MASK";
    static constexpr const char* kNoMaskDetectionMode = "NO_MASK";

private:
    std::string buildDetectionMode() const;

    static void deserializeDetectionModeOrThrow(
        const char* serializedDetectionMode,
        DetectionMode* outDetectionMode);
};

//-------------------------------------------------------------------------------------------------

struct BoxTemperatureDetection: public SettingGroup
{
    UnnamedRect unnamedRect;
    std::string temperatureType = "Maximum";
    std::string detectionType = "Above";
    int thresholdTemperature = 90;
    int duration = 1;
    int areaEmissivity = 50;

    static constexpr int kStartServerRoiIndexFrom = 1;
    static constexpr int kStartDeviceRoiIndexFrom = 1;

    enum class KeyIndex
    {
        unnamedRect,
        temperatureType,
        detectionType,
        thresholdTemperature,
        duration,
        areaEmissivity
    };
    static constexpr const char* kKeys[] = {
        "BoxTemperatureDetection.Area#.Points",
        "BoxTemperatureDetection.Area#.TemperatureType",
        "BoxTemperatureDetection.Area#.DetectionType",
        "BoxTemperatureDetection.Area#.ThresholdTemperature",
        "BoxTemperatureDetection.Area#.Duration",
        "BoxTemperatureDetection.Area#.AreaEmissivity"
    };
    static constexpr const char* kJsonEventName = "BoxTemperatureDetection";
    static constexpr const char* kSunapiEventName = "boxtemperaturedetection";

    BoxTemperatureDetection(const SettingsCapabilities& settingsCapabilities,
        const RoiResolution& roiResolution,
        int roiIndex = -1)
        :
        SettingGroup(settingsCapabilities,
            roiResolution,
            kKeys,
            roiIndex,
            kStartServerRoiIndexFrom,
            kStartDeviceRoiIndexFrom)
    {
    }

    bool operator==(const BoxTemperatureDetection& rhs) const;
    bool operator!=(const BoxTemperatureDetection& rhs) const { return !(*this == rhs); }
    void assignExclusiveFrom(const BoxTemperatureDetection& other){};

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    // The following functions perhaps should be moved to the inheritor-class.
    // The idea is that the current class interacts with the server only,
    // and the inheritor interacts with the device.
    // The decision will be made during other plugins construction.
    void readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo);
    std::string buildDeviceWritingQuery(int channelNumber) const;
};

//-------------------------------------------------------------------------------------------------

struct BoxTemperatureDetectionToggle: public SettingGroup
{
    bool enabled = false;

    enum class KeyIndex
    {
        enabled,
    };
    static constexpr const char* kKeys[] = {
        "BoxTemperatureDetection.Enable",
    };
    static constexpr const char* kJsonEventName = "BoxTemperatureDetection";
    static constexpr const char* kSunapiEventName = "boxtemperaturedetection";

    BoxTemperatureDetectionToggle(const SettingsCapabilities& settingsCapabilities,
        const RoiResolution& roiResolution,
        int /*roiIndex*/ = -1)
        :
        SettingGroup(settingsCapabilities, roiResolution, kKeys)
    {
    }
    bool operator==(const BoxTemperatureDetectionToggle& rhs) const;
    bool operator!=(const BoxTemperatureDetectionToggle& rhs) const { return !(*this == rhs); }
    void assignExclusiveFrom(const BoxTemperatureDetectionToggle& other){};

    void readExclusiveFromServer(const nx::sdk::IStringMap* settings) {}
    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    // The following functions perhaps should be moved to the inheritor-class.
    // The idea is that the current class interacts with the server only,
    // and the inheritor interacts with the device.
    // The decision will be made during other plugins construction.
    void readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo);
    std::string buildDeviceWritingQuery(int channelNumber) const;
};

//-------------------------------------------------------------------------------------------------

struct TemperatureChangeDetection : public SettingGroup
{
    UnnamedRect unnamedRect;
    std::string temperatureType = "Maximum";
    int temperatureGap = 40;
    int duration = 1;

    static constexpr int kStartServerRoiIndexFrom = 1;
    static constexpr int kStartDeviceRoiIndexFrom = 1;

    enum class KeyIndex
    {
        unnamedRect,
        temperatureType,
        temperatureGap,
        duration,
    };
    static constexpr const char* kKeys[] = {
        "TemperatureChangeDetection.Area#.Points",
        "TemperatureChangeDetection.Area#.TemperatureType",
        "TemperatureChangeDetection.Area#.TemperatureGap",
        "TemperatureChangeDetection.Area#.Duration"

    };
    static constexpr const char* kJsonEventName = "TemperatureChangeDetection";
    static constexpr const char* kSunapiEventName = "temperaturechangedetection";

    TemperatureChangeDetection(const SettingsCapabilities& settingsCapabilities,
        const RoiResolution& roiResolution,
        int roiIndex = -1)
        :
        SettingGroup(settingsCapabilities,
            roiResolution,
            kKeys,
            roiIndex,
            kStartServerRoiIndexFrom,
            kStartDeviceRoiIndexFrom)
    {
    }

    bool operator==(const TemperatureChangeDetection& rhs) const;
    bool operator!=(const TemperatureChangeDetection& rhs) const { return !(*this == rhs); }
    void assignExclusiveFrom(const TemperatureChangeDetection& other) {}

    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    void readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo);
    std::string buildDeviceWritingQuery(int channelNumber) const;
};

//-------------------------------------------------------------------------------------------------

struct TemperatureChangeDetectionToggle: public SettingGroup
{
    bool enabled = false;

    enum class KeyIndex
    {
        enabled,
    };
    static constexpr const char* kKeys[] = {
        "TemperatureCHangeDetection.Enable",
    };
    static constexpr const char* kJsonEventName = "TemperatureCHangeDetection";
    static constexpr const char* kSunapiEventName = "temperaturechangedetection";

    TemperatureChangeDetectionToggle(const SettingsCapabilities& settingsCapabilities,
        const RoiResolution& roiResolution,
        int /*roiIndex*/ = -1)
        :
        SettingGroup(settingsCapabilities, roiResolution, kKeys)
    {
    }
    bool operator==(const TemperatureChangeDetectionToggle& rhs) const;
    bool operator!=(const TemperatureChangeDetectionToggle& rhs) const { return !(*this == rhs); }
    void assignExclusiveFrom(const TemperatureChangeDetectionToggle& other) {};

    void readExclusiveFromServer(const nx::sdk::IStringMap* settings) {}
    void readFromServerOrThrow(const nx::sdk::IStringMap* settings, int /*roiIndex*/ = -1);
    void writeToServer(nx::sdk::SettingsResponse* settings, int /*roiIndex*/ = -1) const;

    // The following functions perhaps should be moved to the inheritor-class.
    // The idea is that the current class interacts with the server only,
    // and the inheritor interacts with the device.
    // The decision will be made during other plugins construction.
    void readFromDeviceReplyOrThrow(const nx::kit::Json& channelInfo);
    std::string buildDeviceWritingQuery(int channelNumber) const;
};

//-------------------------------------------------------------------------------------------------
} // namespace nx::vms_server_plugins::analytics::hanwha
