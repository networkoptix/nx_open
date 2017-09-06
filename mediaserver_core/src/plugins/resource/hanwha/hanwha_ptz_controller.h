#pragma once

#if defined(ENABLE_HANWHA)

#include <core/ptz/basic_ptz_controller.h>
#include <core/resource/resource_fwd.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

static const std::map<QString, Ptz::Capability> kHanwhaPtzCapabilityAttributes =
{
    {lit("Absolute.Pan"), Ptz::Capability::AbsolutePanCapability},
    {lit("Absolute.Tilt"), Ptz::Capability::AbsoluteTiltCapability},
    {lit("Absolute.Zoom"), Ptz::Capability::AbsoluteZoomCapability},
    {lit("Continuous.Pan"), Ptz::Capability::ContinuousPanCapability},
    {lit("Continuous.Tilt"), Ptz::Capability::ContinuousTiltCapability},
    {lit("Continuous.Zoom"), Ptz::Capability::ContinuousZoomCapability},
    {lit("Continuous.Focus"), Ptz::Capability::ContinuousFocusCapability},
    {lit("Preset"), Ptz::Capability::NativePresetsPtzCapability},
    {lit("AreaZoom"), Ptz::Capability::ViewportPtzCapability},
    {lit("Home"), Ptz::Capability::HomePtzCapability}
};

class HanwhaPtzController: public QnBasicPtzController
{
    Q_OBJECT
        using base_type = QnAbstractPtzController;

public:
    HanwhaPtzController(const HanwhaResourcePtr& resource);
    virtual Ptz::Capabilities getCapabilities() const override;
    void setPtzCapabilities(Ptz::Capabilities capabilities);
    void setPtzLimits(const QnPtzLimits& limits);

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

private:
    QString channel() const;
    QString freePresetNumber() const;
    QVector3D toHanwhaSpeed(const QVector3D& speed) const;
    QVector3D toHanwhaPosition(const QVector3D& position) const;
    QString toHanwhaFocusCommand(qreal speed) const;
    QString presetNumberFromId(const QString& presetId) const;

private:
    using PresetNumber = QString;
    using PresetId = QString;

    HanwhaResourcePtr m_hanwhaResource;
    Ptz::Capabilities m_ptzCapabilities = Ptz::NoPtzCapabilities;
    QnPtzLimits m_ptzLimits;

    mutable std::map<PresetId, PresetNumber> m_presetNumberById;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)
