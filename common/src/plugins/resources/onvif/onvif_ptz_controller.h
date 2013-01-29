#ifndef QN_ONVIF_PTZ_CONTROLLER_H
#define QN_ONVIF_PTZ_CONTROLLER_H

#include <QtCore/QMutex>
#include <QtCore/QPair>

#include "core/resource/resource_fwd.h"
#include "core/resource/interface/abstract_ptz_controller.h"

class QnPtzSpaceMapper;

class QnOnvifPtzController: public QnAbstractPtzController {
    Q_OBJECT
public:
    QnOnvifPtzController(const QnPlOnvifResourcePtr &resource);

    virtual int startMove(qreal xVelocity, qreal yVelocity, qreal zoomVelocity) override;
    virtual int stopMove() override;
    virtual int moveTo(qreal xPos, qreal yPos, qreal zoomPos) override;
    virtual int getPosition(qreal *xPos, qreal *yPos, qreal *zoomPos) override;
    virtual Qn::CameraCapabilities getCapabilities() override;
    virtual const QnPtzSpaceMapper *getSpaceMapper() override;

    QString getPtzConfigurationToken();
    void setMediaProfileToken(const QString& value);

private:
    double normalizeSpeed(qreal inputVelocity, const QPair<qreal, qreal>& nativeCoeff, qreal userCoeff);

private:
    QnPlOnvifResourcePtr m_resource;
    Qn::CameraCapabilities m_capabilities;
    QString m_mediaProfile;
    QString m_ptzConfigurationToken;
    QPair<qreal, qreal> m_xNativeVelocityCoeff; // first for positive value, second for negative
    QPair<qreal, qreal> m_yNativeVelocityCoeff;
    QPair<qreal, qreal> m_zoomNativeVelocityCoeff;
    mutable QMutex m_mutex;
};

#endif // QN_ONVIF_PTZ_CONTROLLER_H
