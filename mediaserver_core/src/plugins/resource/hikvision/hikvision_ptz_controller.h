#pragma once

#include <core/ptz/basic_ptz_controller.h>
#include <plugins/utils/xml_request_helper.h>

namespace nx {
namespace mediaserver_core {
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

private:
    mutable nx::plugins::utils::XmlRequestHelper m_client;
    std::optional<int> m_channel;
    Ptz::Capabilities m_capabilities = Ptz::NoPtzCapabilities;
};

} // namespace hikvision
} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
