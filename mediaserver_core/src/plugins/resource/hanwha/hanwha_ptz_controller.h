#pragma once

#include <plugins/resource/hanwha/hanwha_mapped_preset_manager.h>

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
    virtual Ptz::Capabilities getCapabilities() const override;
    void setPtzCapabilities(Ptz::Capabilities capabilities);
    void setPtzLimits(const QnPtzLimits& limits);
    void setPtzTraits(const QnPtzAuxilaryTraitList& traits);

    virtual bool continuousMove(const QVector3D& speed) override;
    virtual bool continuousFocus(qreal speed) override;
    virtual bool absoluteMove(
        Qn::PtzCoordinateSpace space,
        const QVector3D& position,
        qreal speed) override;
    virtual bool viewportMove(qreal aspectRatio, const QRectF& viewport, qreal speed) override;

    virtual bool getPosition(Qn::PtzCoordinateSpace space, QVector3D* position) const override;
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
    QVector3D toHanwhaSpeed(const QVector3D& speed) const;
    QVector3D toHanwhaPosition(const QVector3D& position) const;
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
    QnPtzLimits m_ptzLimits;
    QnPtzAuxilaryTraitList m_ptzTraits;
    mutable std::unique_ptr<HanwhaMappedPresetManager> m_presetManager;
    QMap<QString, float> m_lastParamValue;

};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
