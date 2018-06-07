#pragma once

#include <plugins/resource/hanwha/hanwha_mapped_preset_manager.h>
#include <plugins/resource/hanwha/hanwha_ptz_executor.h>
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

    virtual Ptz::Capabilities getCapabilities() const override;
    virtual Ptz::Capabilities alternativeCapabilities() const override;

    void setPtzCapabilities(Ptz::Capabilities capabilities);
    void setAlternativePtzCapabilities(Ptz::Capabilities capabilities);
    void setPtzLimits(const QnPtzLimits& limits);
    void setPtzTraits(const QnPtzAuxilaryTraitList& traits);
    void setAlternativePtzRanges(const std::map<QString, HanwhaRange>& ranges);

    virtual bool continuousMove(const nx::core::ptz::Vector& speedVector) override;
    virtual bool continuousFocus(qreal speed) override;
    virtual bool absoluteMove(
        Qn::PtzCoordinateSpace space,
        const nx::core::ptz::Vector& position,
        qreal speed) override;

    virtual bool viewportMove(qreal aspectRatio, const QRectF& viewport, qreal speed) override;

    virtual bool getPosition(
        Qn::PtzCoordinateSpace space,
        nx::core::ptz::Vector* outPosition) const override;

    virtual bool getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits* limits) const override;
    virtual bool getFlip(Qt::Orientations* flip) const override;

    virtual bool createPreset(const QnPtzPreset& preset) override;
    virtual bool updatePreset(const QnPtzPreset& preset) override;
    virtual bool removePreset(const QString& presetId) override;
    virtual bool activatePreset(const QString& presetId, qreal speed) override;
    virtual bool getPresets(QnPtzPresetList* presets) const override;

    virtual bool getAuxilaryTraits(QnPtzAuxilaryTraitList* auxilaryTraits) const override;
    virtual bool runAuxilaryCommand(
        const QnPtzAuxilaryTrait& trait,
        const QString& data) override;

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
    std::unique_ptr<HanwhaPtzExecutor> m_alternativePtzExecutor;

};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
