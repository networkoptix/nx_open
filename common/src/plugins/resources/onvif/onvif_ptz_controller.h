#ifndef QN_ONVIF_PTZ_CONTROLLER_H
#define QN_ONVIF_PTZ_CONTROLLER_H

#ifndef DISABLE_ONVIF

#include <QtCore/QMutex>
#include <QtCore/QPair>
#include <QtGui/QMatrix4x4>

#include "core/resource/resource_fwd.h"
#include "core/resource/interface/abstract_ptz_controller.h"

class QnPtzSpaceMapper;

class QnOnvifPtzController: public QnAbstractPtzController {
    Q_OBJECT
public:
    QnOnvifPtzController(QnPlOnvifResource* resource);

    virtual int startMove(qreal xVelocity, qreal yVelocity, qreal zoomVelocity) override;
    virtual int stopMove() override;
    virtual int moveTo(qreal xPos, qreal yPos, qreal zoomPos) override;
    virtual int getPosition(qreal *xPos, qreal *yPos, qreal *zoomPos) override;
    virtual Qn::PtzCapabilities getCapabilities() override;
    virtual const QnPtzSpaceMapper *getSpaceMapper() override;

    // TODO: #Elric need to implement this one properly.
    void setFlipped(bool horizontal, bool vertical);
    void getFlipped(bool *horizontal, bool *vertical);

    QString getPtzConfigurationToken();
    void setMediaProfileToken(const QString& value);

private:
    double normalizeSpeed(qreal inputVelocity, const QPair<qreal, qreal>& nativeCoeff, qreal userCoeff);

private:
    QnPlOnvifResource* m_resource;
    Qn::PtzCapabilities m_ptzCapabilities;
    const QnPtzSpaceMapper *m_ptzMapper;
    QString m_mediaProfile;
    QString m_ptzConfigurationToken;
    QPair<qreal, qreal> m_xNativeVelocityCoeff; // first for positive value, second for negative
    QPair<qreal, qreal> m_yNativeVelocityCoeff;
    QPair<qreal, qreal> m_zoomNativeVelocityCoeff;
    mutable QMutex m_mutex;

    bool m_verticalFlipped, m_horizontalFlipped;
};

#endif //DISABLE_ONVIF

#endif // QN_ONVIF_PTZ_CONTROLLER_H
