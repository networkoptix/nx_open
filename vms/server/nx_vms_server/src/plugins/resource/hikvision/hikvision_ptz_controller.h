#pragma once

#include <core/ptz/basic_ptz_controller.h>
#include <core/resource/json_resource_property.h>
#include <plugins/utils/xml_request_helper.h>

namespace nx {
namespace vms::server {
namespace plugins {
namespace hikvision {

class IsapiPtzController: public QnBasicPtzController
{
    typedef QnBasicPtzController base_type;

public:
    IsapiPtzController(const QnResourcePtr& resource, QAuthenticator authenticator);

    virtual Ptz::Capabilities getCapabilities(
        const nx::core::ptz::Options& options) const override;

    virtual bool continuousMove(
        const nx::core::ptz::Vector& speed,
        const nx::core::ptz::Options& options) override;

    virtual bool absoluteMove(
        Qn::PtzCoordinateSpace space,
        const nx::core::ptz::Vector& position,
        qreal speed,
        const nx::core::ptz::Options& options) override;

    virtual bool getPosition(
        Qn::PtzCoordinateSpace space,
        nx::core::ptz::Vector* position,
        const nx::core::ptz::Options& options) const override;

    virtual bool getLimits(
        Qn::PtzCoordinateSpace space,
        QnPtzLimits* limits,
        const nx::core::ptz::Options& options) const override;

    virtual bool getPresets(QnPtzPresetList* presets) const override;
    virtual bool activatePreset(const QString& presetId, qreal speed) override;
    virtual bool createPreset(const QnPtzPreset& preset) override;
    virtual bool updatePreset(const QnPtzPreset& preset) override;
    virtual bool removePreset(const QString& presetId) override;

private:
    QString url(const QString& action = {}) const;
    bool isSupported(
        const nx::core::ptz::Options& options,
        Qn::PtzCoordinateSpace space = Qn::DevicePtzCoordinateSpace) const;

    void loadCapabilities(
        const nx::plugins::utils::XmlRequestHelper::Result& capabilities);

    std::optional<std::map<int, QString>> readCameraPresets() const;

private:
    mutable QnMutex m_mutex;
    mutable nx::plugins::utils::XmlRequestHelper m_client;

    std::optional<int> m_channel;
    Ptz::Capabilities m_capabilities = Ptz::NoPtzCapabilities;
    QnPtzLimits m_limits;

    int m_maxPresetNumber = 0;
    bool m_isPresetNameSupported = false;
    std::set<int> m_specialPresetNumbers;
    mutable QnJsonResourceProperty<std::map<int /*idOnCamera*/, QnPtzPreset>> m_presetMappings;
};

} // namespace hikvision
} // namespace plugins
} // namespace vms::server
} // namespace nx
