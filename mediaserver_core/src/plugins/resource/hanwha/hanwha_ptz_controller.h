#pragma once

#include <plugins/resource/hanwha/hanwha_mapped_preset_manager.h>
#include <plugins/resource/hanwha/hanwha_ptz_command_streamer.h>
#include <plugins/resource/hanwha/hanwha_common.h>
#include <plugins/resource/hanwha/hanwha_range.h>

#include <core/ptz/basic_ptz_controller.h>
#include <core/resource/resource_fwd.h>
#include <core/ptz/ptz_preset.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

class HanwhaPtzController: public QnBasicPtzController
{
    Q_OBJECT
    using base_type = QnAbstractPtzController;

public:
    using DevicePresetId = QString;
    using NxPresetId = QString;

public:
    HanwhaPtzController(const HanwhaResourcePtr& resource);
    virtual ~HanwhaPtzController() override;

    virtual Ptz::Capabilities getCapabilities(
        const nx::core::ptz::Options& options) const override;

    void setPtzCapabilities(Ptz::Capabilities capabilities);
    void setAlternativePtzCapabilities(Ptz::Capabilities capabilities);
    void setPtzLimits(const QnPtzLimits& limits);
    void setPtzTraits(const QnPtzAuxilaryTraitList& traits);
    void setAlternativePtzRanges(const std::map<QString, HanwhaRange>& ranges);

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

    virtual bool getAuxilaryTraits(
        QnPtzAuxilaryTraitList* auxilaryTraits,
        const nx::core::ptz::Options& options) const override;

    virtual bool runAuxilaryCommand(
        const QnPtzAuxilaryTrait& trait,
        const QString& data,
        const nx::core::ptz::Options& options) override;

private:
    QString channel() const;
    nx::core::ptz::Vector toHanwhaSpeed(const nx::core::ptz::Vector& speed) const;
    nx::core::ptz::Vector toHanwhaPosition(const nx::core::ptz::Vector& position) const;
    QString toHanwhaFocusCommand(qreal speed) const;
    std::map<QString, QString> makeViewPortParameters(
        qreal aspectRatio,
        const QRectF rect) const;

private:
    using PresetNumber = QString;
    using PresetId = QString;

    mutable QnMutex m_mutex;
    HanwhaResourcePtr m_hanwhaResource;
    Ptz::Capabilities m_ptzCapabilities = Ptz::NoPtzCapabilities;
    Ptz::Capabilities m_alternativePtzCapabilities = Ptz::NoPtzCapabilities;
    QnPtzLimits m_ptzLimits;
    QnPtzAuxilaryTraitList m_ptzTraits;
    mutable std::unique_ptr<HanwhaMappedPresetManager> m_presetManager;
    QMap<QString, float> m_lastParamValue;
    std::unique_ptr<HanwhaPtzCommandStreamer> m_commandStreamer;

};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
