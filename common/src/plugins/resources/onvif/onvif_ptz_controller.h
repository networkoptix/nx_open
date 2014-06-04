#ifndef QN_ONVIF_PTZ_CONTROLLER_H
#define QN_ONVIF_PTZ_CONTROLLER_H

#ifdef ENABLE_ONVIF

#include <QtCore/QMutex>
#include <QtCore/QPair>

#include <core/ptz/basic_ptz_controller.h>

class QnPtzSpaceMapper;

class QnOnvifPtzController: public QnBasicPtzController {
    Q_OBJECT
    typedef QnBasicPtzController base_type;

public:
    QnOnvifPtzController(const QnPlOnvifResourcePtr &resource);
    virtual ~QnOnvifPtzController();

    virtual Qn::PtzCapabilities getCapabilities() override;
    
    virtual bool continuousMove(const QVector3D &speed) override;
    virtual bool continuousFocus(qreal speed) override;
    virtual bool absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) override;
    
    virtual bool getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) override;
    virtual bool getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits *limits) override;
    virtual bool getFlip(Qt::Orientations *flip) override;

private:
    struct SpeedLimits {
        SpeedLimits(): min(0.0), max(0.0) {}
        SpeedLimits(qreal min, qreal max): min(min), max(max) {}

        qreal min;
        qreal max;
    };

    static double normalizeSpeed(qreal speed, const SpeedLimits &speedLimits);
    
    Qn::PtzCapabilities initContinuousMove();
    Qn::PtzCapabilities initContinuousFocus();

    bool stopInternal();
    bool moveInternal(const QVector3D &speed);

private:
    QnPlOnvifResourcePtr m_resource;
    Qn::PtzCapabilities m_capabilities;
    bool m_stopBroken;

    SpeedLimits m_panSpeedLimits, m_tiltSpeedLimits, m_zoomSpeedLimits, m_focusSpeedLimits;
    QnPtzLimits m_limits;
};

#endif //ENABLE_ONVIF

#endif // QN_ONVIF_PTZ_CONTROLLER_H
