#ifndef QN_ONVIF_PTZ_CONTROLLER_H
#define QN_ONVIF_PTZ_CONTROLLER_H

#ifdef ENABLE_ONVIF

#include <nx/utils/thread/mutex.h>
#include <QtCore/QPair>

#include <core/ptz/basic_ptz_controller.h>
#include <nx/vms/server/resource/resource_fwd.h>

class QnPtzSpaceMapper;

class QnOnvifPtzController: public QnBasicPtzController {
    Q_OBJECT
    typedef QnBasicPtzController base_type;

public:
    QnOnvifPtzController(const QnPlOnvifResourcePtr &resource);

    virtual Ptz::Capabilities getCapabilities(const nx::core::ptz::Options& options) const override;

    virtual bool continuousMove(
        const nx::core::ptz::Vector& speed,
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

    virtual bool getPosition(
        Qn::PtzCoordinateSpace space,
        nx::core::ptz::Vector* outPosition,
        const nx::core::ptz::Options& options) const override;

    virtual bool getLimits(
        Qn::PtzCoordinateSpace space,
        QnPtzLimits *limits,
        const nx::core::ptz::Options& options) const override;

    virtual bool getFlip(
        Qt::Orientations *flip,
        const nx::core::ptz::Options& options) const override;

    virtual bool getPresets(QnPtzPresetList *presets) const override;
    virtual bool activatePreset(const QString &presetId, qreal speed) override;
    virtual bool createPreset(const QnPtzPreset &preset) override;
    virtual bool updatePreset(const QnPtzPreset &preset) override;
    virtual bool removePreset(const QString &presetId) override;

private:
    struct SpeedLimits {
        SpeedLimits(): min(0.0), max(0.0) {}
        SpeedLimits(qreal min, qreal max): min(min), max(max) {}

        qreal min;
        qreal max;
    };

    static double normalizeSpeed(qreal speed, const SpeedLimits &speedLimits);

    Ptz::Capabilities initMove();
    Ptz::Capabilities initContinuousFocus();
    bool readBuiltinPresets();

    bool stopInternal();
    bool moveInternal(const nx::core::ptz::Vector& speedVector);
    QString presetToken(const QString &presetId);
    QString presetName(const QString &presetId);

private:
    QnPlOnvifResourcePtr m_resource;
    Ptz::Capabilities m_capabilities = Ptz::NoPtzCapabilities;
    bool m_stopBroken = false;
    bool m_speedBroken = false;

    SpeedLimits m_panSpeedLimits, m_tiltSpeedLimits, m_zoomSpeedLimits, m_focusSpeedLimits;
    QnPtzLimits m_limits;
    QMap<QString, QString> m_presetTokenById;
    QMap<QString, QString> m_presetNameByToken;
    bool m_ptzPresetsIsRead = false;
    char m_floatFormat[16];
    char m_doubleFormat[16];

    mutable QnMutex m_mutex;
};

#endif //ENABLE_ONVIF

#endif // QN_ONVIF_PTZ_CONTROLLER_H
