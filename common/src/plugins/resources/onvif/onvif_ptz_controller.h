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
    virtual bool absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) override;
    
    virtual bool getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) override;
    virtual bool getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits *limits) override;
    virtual bool getFlip(Qt::Orientations *flip) override;

private:
    double normalizeSpeed(qreal inputVelocity, const QPair<qreal, qreal>& nativeCoeff, qreal userCoeff);
    
    void initCoefficients();
    bool stopInternal();
    bool moveInternal(const QVector3D &speed);

private:
    QnPlOnvifResourcePtr m_resource;
    Qn::PtzCapabilities m_capabilities;
    bool m_stopBroken;
    bool m_panFlipped;

    QPair<qreal, qreal> m_xNativeVelocityCoeff; // first for positive value, second for negative
    QPair<qreal, qreal> m_yNativeVelocityCoeff;
    QPair<qreal, qreal> m_zoomNativeVelocityCoeff;

    QnPtzLimits m_limits;
};

#endif //ENABLE_ONVIF

#endif // QN_ONVIF_PTZ_CONTROLLER_H
