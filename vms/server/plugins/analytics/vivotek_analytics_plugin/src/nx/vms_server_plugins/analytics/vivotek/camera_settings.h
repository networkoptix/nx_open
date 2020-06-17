#pragma once

#include <optional>
#include <variant>
#include <utility>

#include <nx/sdk/ptr.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/analytics/rect.h>
#include <nx/utils/url.h>

#include <QtCore/QJsonValue>

#include "camera_features.h"
#include "named_polygon.h"
#include "named_line.h"

namespace nx::vms_server_plugins::analytics::vivotek {

class CameraSettings
{
public:
    template <typename Value_>
    class Entry
    {
    public:
        using Value = Value_;

    public:
        bool hasNothing() const
        {
            return m_state.index() == nothingIndex;
        }

        void emplaceNothing()
        {
            m_state.template emplace<nothingIndex>();
        }

        bool hasValue() const
        {
            return m_state.index() == valueIndex;
        }

        const Value& value() const
        {
            return std::get<valueIndex>(m_state);
        }

        template <typename... Args>
        Value& emplaceValue(Args&&... args)
        {
            return m_state.template emplace<valueIndex>(std::forward<Args>(args)...);
        }

        bool hasError() const
        {
            return m_state.index() == errorIndex;
        }

        const QString& errorMessage() const
        {
            return std::get<errorIndex>(m_state);
        }

        template <typename... Args>
        QString& emplaceErrorMessage(Args&&... args)
        {
            return m_state.template emplace<errorIndex>(std::forward<Args>(args)...);
        }

    private:
        static constexpr std::size_t nothingIndex = 0;
        static constexpr std::size_t valueIndex = 1;
        static constexpr std::size_t errorIndex = 2;
        std::variant<std::monostate, Value, QString> m_state;
    };

public:
    #define NX_CAMERA_SETTINGS_ENTRY(NAME_PREFIX, NAME, ...) \
        struct NAME: Entry<__VA_ARGS__> \
        { \
            using Base = Entry<__VA_ARGS__>; \
            \
            using Base::Base; \
            using Base::operator=; \
            \
            static constexpr char name[] = #NAME_PREFIX "." #NAME; \
        }

    struct Vca
    {
        NX_CAMERA_SETTINGS_ENTRY(Vca, Enabled, bool) enabled;

        NX_CAMERA_SETTINGS_ENTRY(Vca, Sensitivity, int) sensitivity;

        struct Installation
        {
            NX_CAMERA_SETTINGS_ENTRY(Vca.Installation, Height, int) height;
            NX_CAMERA_SETTINGS_ENTRY(Vca.Installation, TiltAngle, int) tiltAngle;
            NX_CAMERA_SETTINGS_ENTRY(Vca.Installation, RollAngle, int) rollAngle;

            struct Exclusion
            {
                NX_CAMERA_SETTINGS_ENTRY(Vca.Installation.Exclusion#, Region, NamedPolygon) region;
            };
            std::vector<Exclusion> exclusions;
        } installation;

        struct CrowdDetection
        {
            struct Rule
            {
                NX_CAMERA_SETTINGS_ENTRY(Vca.CrowdDetection.Rule#, Region, NamedPolygon) region;

                NX_CAMERA_SETTINGS_ENTRY(Vca.CrowdDetection.Rule#, SizeThreshold,
                    int) sizeThreshold;

                NX_CAMERA_SETTINGS_ENTRY(Vca.CrowdDetection.Rule#, EnterDelay, int) enterDelay;
                NX_CAMERA_SETTINGS_ENTRY(Vca.CrowdDetection.Rule#, ExitDelay, int) exitDelay;
            };
            std::vector<Rule> rules;
        };
        std::optional<CrowdDetection> crowdDetection;

        struct LoiteringDetection
        {
            struct Rule
            {
                NX_CAMERA_SETTINGS_ENTRY(Vca.LoiteringDetection.Rule#, Region, NamedPolygon) region;
                NX_CAMERA_SETTINGS_ENTRY(Vca.LoiteringDetection.Rule#, Delay, int) delay;
            };
            std::vector<Rule> rules;
        };
        std::optional<LoiteringDetection> loiteringDetection;

        struct IntrusionDetection
        {
            struct Rule
            {
                NX_CAMERA_SETTINGS_ENTRY(Vca.IntrusionDetection.Rule#, Region, NamedPolygon) region;
                NX_CAMERA_SETTINGS_ENTRY(Vca.IntrusionDetection.Rule#, Inverted, bool) inverted;
            };
            std::vector<Rule> rules;
        };
        std::optional<IntrusionDetection> intrusionDetection;

        struct LineCrossingDetection
        {
            struct Rule
            {
                NX_CAMERA_SETTINGS_ENTRY(Vca.LineCrossingDetection.Rule#, Line, NamedLine) line;
            };
            std::vector<Rule> rules;
        };
        std::optional<LineCrossingDetection> lineCrossingDetection;

        struct MissingObjectDetection
        {
            struct Rule
            {
                NX_CAMERA_SETTINGS_ENTRY(Vca.MissingObjectDetection.Rule#, Region,
                    NamedPolygon) region;

                NX_CAMERA_SETTINGS_ENTRY(Vca.MissingObjectDetection.Rule#, MinSize,
                    nx::sdk::analytics::Rect) minSize;

                NX_CAMERA_SETTINGS_ENTRY(Vca.MissingObjectDetection.Rule#, MaxSize,
                    nx::sdk::analytics::Rect) maxSize;

                NX_CAMERA_SETTINGS_ENTRY(Vca.MissingObjectDetection.Rule#, Delay, int) delay;

                NX_CAMERA_SETTINGS_ENTRY(
                    Vca.MissingObjectDetection.Rule#, RequiresHumanInvolvement,
                    bool) requiresHumanInvolvement;
            };
            std::vector<Rule> rules;
        };
        std::optional<MissingObjectDetection> missingObjectDetection;

        struct UnattendedObjectDetection
        {
            struct Rule
            {
                NX_CAMERA_SETTINGS_ENTRY(Vca.UnattendedObjectDetection.Rule#, Region,
                    NamedPolygon) region;

                NX_CAMERA_SETTINGS_ENTRY(Vca.UnattendedObjectDetection.Rule#, MinSize,
                    nx::sdk::analytics::Rect) minSize;

                NX_CAMERA_SETTINGS_ENTRY(Vca.UnattendedObjectDetection.Rule#, MaxSize,
                    nx::sdk::analytics::Rect) maxSize;

                NX_CAMERA_SETTINGS_ENTRY(Vca.UnattendedObjectDetection.Rule#, Delay, int) delay;

                NX_CAMERA_SETTINGS_ENTRY(
                    Vca.UnattendedObjectDetection.Rule#, RequiresHumanInvolvement,
                    bool) requiresHumanInvolvement;
            };
            std::vector<Rule> rules;
        };
        std::optional<UnattendedObjectDetection> unattendedObjectDetection;

        struct FaceDetection
        {
            struct Rule
            {
                NX_CAMERA_SETTINGS_ENTRY(Vca.FaceDetection.Rule#, Region, NamedPolygon) region;
            };
            std::vector<Rule> rules;
        };
        std::optional<FaceDetection> faceDetection;

        struct RunningDetection
        {
            struct Rule
            {
                NX_CAMERA_SETTINGS_ENTRY(Vca.RunningDetection.Rule#, Name, QString) name;
                NX_CAMERA_SETTINGS_ENTRY(Vca.RunningDetection.Rule#, MinCount, int) minCount;
                NX_CAMERA_SETTINGS_ENTRY(Vca.RunningDetection.Rule#, MinSpeed, int) minSpeed;
                NX_CAMERA_SETTINGS_ENTRY(Vca.RunningDetection.Rule#, Delay, int) delay;
            };
            std::vector<Rule> rules;
        };
        std::optional<RunningDetection> runningDetection;
    };
    std::optional<Vca> vca;

    #undef NX_CAMERA_SETTINGS_ENTRY

public:
    explicit CameraSettings(const CameraFeatures& features);

    void fetchFrom(const nx::utils::Url& cameraUrl);
    void storeTo(const nx::utils::Url& cameraUrl);

    void parseFromServer(const nx::sdk::IStringMap& values);
    nx::sdk::Ptr<nx::sdk::StringMap> serializeToServer();

    nx::sdk::Ptr<nx::sdk::StringMap> getErrorMessages() const;

    static QJsonValue getModelForManifest(const CameraFeatures& features);
};

} // namespace nx::vms_server_plugins::analytics::vivotek
