#ifndef __ONVIF_PTZ_CONTROLLER_H__
#define __ONVIF_PTZ_CONTROLLER_H__

#include <QMutex>
#include "core/resource/resource_fwd.h"
#include "core/resource/interface/abstract_ptz_controller.h"

class QnOnvifPtzController: public QnAbstractPtzController
{
public:
    QnOnvifPtzController(QnResourcePtr res, const QString& mediaProfile);

    virtual int startMove(qreal xVelocity, qreal yVelocity, qreal zoomVelocity) override;
    virtual int stopMove() override;

    QString getPtzNodeToken() const;
    QString getPtzConfigurationToken();
    void setMediaProfileToken(const QString& value);
private:
    float normalizeSpeed(qreal inputVelocity, const QPair<qreal, qreal>& nativeCoeff, qreal userCoeff);
private:
    QnPlOnvifResourcePtr m_res;
    QString m_ptzToken;
    QString m_mediaProfile;
    QString m_ptzConfigurationToken;
    QPair<qreal, qreal> m_xNativeVelocityCoeff; // first for positive value, second for negative
    QPair<qreal, qreal> m_yNativeVelocityCoeff;
    QPair<qreal, qreal> m_zoomNativeVelocityCoeff;
    mutable QMutex m_mutex;
};

#endif // __ONVIF_PTZ_CONTROLLER_H__
