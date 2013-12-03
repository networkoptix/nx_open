#ifndef QN_ONVIF_PTZ_CONTROLLER_H
#define QN_ONVIF_PTZ_CONTROLLER_H

#ifdef ENABLE_ONVIF

#include <QtCore/QMutex>
#include <QtCore/QPair>
#include <QtGui/QMatrix4x4>

#include "core/resource/resource_fwd.h"
#include "core/ptz/abstract_ptz_controller.h"

class QnPtzSpaceMapper;

class QnOnvifPtzController: public QnAbstractPtzController {
    Q_OBJECT
public:
    QnOnvifPtzController(const QnPlOnvifResourcePtr &resource);

    virtual Qn::PtzCapabilities getCapabilities() override;
    virtual bool continuousMove(const QVector3D &speed) override;
    virtual bool absoluteMove(const QVector3D &position) override;
    virtual bool getPosition(QVector3D *position) override;
    virtual bool getLimits(QnPtzLimits *limits) override;
    virtual bool getFlip(Qt::Orientations *flip) override;
    virtual bool relativeMove(qreal aspectRatio, const QRectF &viewport) override;

private:
    double normalizeSpeed(qreal inputVelocity, const QPair<qreal, qreal>& nativeCoeff, qreal userCoeff);
    
    bool stopInternal();
    bool moveInternal(const QVector3D &speed);

private:
    mutable QMutex m_mutex;
    QnPlOnvifResourcePtr m_resource;
    Qn::PtzCapabilities m_ptzCapabilities;
    bool m_stopBroken;

    QPair<qreal, qreal> m_xNativeVelocityCoeff; // first for positive value, second for negative
    QPair<qreal, qreal> m_yNativeVelocityCoeff;
    QPair<qreal, qreal> m_zoomNativeVelocityCoeff;
};

#endif //ENABLE_ONVIF

#endif // QN_ONVIF_PTZ_CONTROLLER_H
