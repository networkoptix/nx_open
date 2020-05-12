#pragma once

#include <optional>
#include <variant>
#include <utility>

#include <nx/sdk/ptr.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/utils/url.h>

#include <QtCore/QJsonValue>

#include "camera_features.h"

namespace nx::vms_server_plugins::analytics::vivotek {

class CameraSettings
{
public:
    template <typename Type>
    class Entry
    {
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

        const Type& value() const
        {
            return std::get<valueIndex>(m_state);
        }

        template <typename... Args>
        Type& emplaceValue(Args&&... args)
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
        std::variant<std::monostate, Type, QString> m_state;
    };

public:
    #define NX_CAMERA_SETTINGS_ENTRY(namePrefix, name_, type) \
        struct name_: Entry<type> \
        { \
            using Base = Entry<type>; \
            \
            using Base::Base; \
            using Base::operator=; \
            \
            static constexpr char name[] = #namePrefix "." #name_; \
        }

    struct Vca
    {
        NX_CAMERA_SETTINGS_ENTRY(Vca, Enabled, bool) enabled;

        struct Installation
        {
            NX_CAMERA_SETTINGS_ENTRY(Vca.Installation, Height, int) height;
            NX_CAMERA_SETTINGS_ENTRY(Vca.Installation, TiltAngle, int) tiltAngle;
            NX_CAMERA_SETTINGS_ENTRY(Vca.Installation, RollAngle, int) rollAngle;
        } installation;

        NX_CAMERA_SETTINGS_ENTRY(Vca, Sensitivity, int) sensitivity;
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
