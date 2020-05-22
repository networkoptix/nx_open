#pragma once

#include <optional>
#include <variant>
#include <utility>

#include <nx/sdk/ptr.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/utils/url.h>

#include <QtCore/QJsonValue>

#include "camera_features.h"
#include "named_polygon.h"

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
        } installation;

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
    };
    std::optional<Vca> vca;

    #undef NX_CAMERA_SETTINGS_ENTRY

public:
    explicit CameraSettings(const CameraFeatures& features);

    void fetchFrom(const nx::utils::Url& cameraUrl);
    void storeTo(const nx::utils::Url& cameraUrl);

    void parseFromServer(const nx::sdk::IStringMap& values);
    nx::sdk::Ptr<nx::sdk::StringMap> unparseToServer();

    nx::sdk::Ptr<nx::sdk::StringMap> getErrorMessages() const;

    static QJsonValue getModelForManifest(const CameraFeatures& features);
};

} // namespace nx::vms_server_plugins::analytics::vivotek
