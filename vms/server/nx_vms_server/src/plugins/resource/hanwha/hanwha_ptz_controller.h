#pragma once

#include <map>

#include <plugins/resource/hanwha/hanwha_mapped_preset_manager.h>
#include <plugins/resource/hanwha/hanwha_ptz_command_streamer.h>
#include <plugins/resource/hanwha/hanwha_common.h>
#include <plugins/resource/hanwha/hanwha_ptz_common.h>
#include <plugins/resource/hanwha/hanwha_range.h>
#include <plugins/resource/hanwha/hanwha_ptz_common.h>

#include <core/ptz/basic_ptz_controller.h>
#include <core/resource/resource_fwd.h>
#include <core/ptz/ptz_preset.h>

#include <nx/utils/std/optional.h>

namespace nx {
namespace vms::server {
namespace plugins {

class HanwhaPtzController: public QnBasicPtzController
{
    Q_OBJECT
    using base_type = QnAbstractPtzController;

public:
    HanwhaPtzController(const HanwhaResourcePtr& resource);
    virtual ~HanwhaPtzController() = default;

    virtual Ptz::Capabilities getCapabilities(
        const nx::core::ptz::Options& options) const override;

    void setPtzCapabilities(const HanwhaPtzCapabilitiesMap& capabilities);
    void setPtzLimits(const QnPtzLimits& limits);
    void setPtzTraits(const QnPtzAuxiliaryTraitList& traits);
    void setPtzRanges(const HanwhaPtzRangeMap& ranges);

    virtual bool continuousMove(
        const nx::core::ptz::Vector& speedVector,
        const nx::core::ptz::Options& options) override;

    virtual bool continuousFocus(
        qreal speed,
        const nx::core::ptz::Options& options) override;

    virtual bool absoluteMove(
        Qn::PtzCoordinateSpace space,
        const nx::core::ptz::Vector& position,
        qreal speed,
        const nx::core::ptz::Options& options) override;

    virtual bool relativeMove(
        const nx::core::ptz::Vector& relativeMovementVector,
        const nx::core::ptz::Options& options) override;

    virtual bool relativeFocus(
        qreal relativeMovement,
        const nx::core::ptz::Options& options) override;

    virtual bool viewportMove(
        qreal aspectRatio,
        const QRectF& viewport,
        qreal speed,
        const nx::core::ptz::Options& options) override;

    virtual bool getPosition(
        Qn::PtzCoordinateSpace space,
        nx::core::ptz::Vector* outPosition,
        const nx::core::ptz::Options& options) const override;

    virtual bool getLimits(
        Qn::PtzCoordinateSpace space,
        QnPtzLimits* limits,
        const nx::core::ptz::Options& options) const override;

    virtual bool getFlip(
        Qt::Orientations* flip,
        const nx::core::ptz::Options& options) const override;

    virtual bool createPreset(const QnPtzPreset& preset) override;
    virtual bool updatePreset(const QnPtzPreset& preset) override;
    virtual bool removePreset(const QString& presetId) override;
    virtual bool activatePreset(const QString& presetId, qreal speed) override;
    virtual bool getPresets(QnPtzPresetList* presets) const override;

    virtual bool getAuxiliaryTraits(
        QnPtzAuxiliaryTraitList* auxiliaryTraits,
        const nx::core::ptz::Options& options) const override;

    virtual bool runAuxiliaryCommand(
        const QnPtzAuxiliaryTrait& trait,
        const QString& data,
        const nx::core::ptz::Options& options) override;

private:
    QString channel() const;
    std::optional<HanwhaRange> range(
        nx::core::ptz::Type ptzType,
        const HanwhaPtzParameterName& parameterName) const;

    nx::core::ptz::Vector toHanwhaSpeed(const nx::core::ptz::Vector& speed) const;
    nx::core::ptz::Vector toHanwhaPosition(const nx::core::ptz::Vector& position) const;
    std::optional<nx::core::ptz::Vector> toHanwhaRelativeMovement(
        const nx::core::ptz::Vector& relativeMovement) const;

    QString toHanwhaFocusCommand(qreal speed) const;
    std::map<QString, QString> makeViewPortParameters(
        qreal aspectRatio,
        const QRectF rect) const;

    bool hasAnyCapability(Ptz::Capabilities capabilities, nx::core::ptz::Type ptzType) const;

    bool useNormalizedSpeed() const;

    QString makeWarningMessage(const QString& text) const;
private:
    mutable QnMutex m_mutex;
    HanwhaResourcePtr m_hanwhaResource;
    HanwhaPtzCapabilitiesMap m_ptzCapabilities;
    QnPtzLimits m_ptzLimits;
    QnPtzAuxiliaryTraitList m_ptzTraits;
    HanwhaPtzRangeMap m_ptzRanges;
    mutable std::unique_ptr<HanwhaMappedPresetManager> m_presetManager;
    QMap<QString, float> m_lastParamValue;
    std::unique_ptr<HanwhaPtzCommandStreamer> m_commandStreamer;
};

} // namespace plugins
} // namespace vms::server
} // namespace nx
