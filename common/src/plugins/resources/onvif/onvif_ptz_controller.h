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

    virtual int startMove(const QVector3D &speed) override;
    virtual int setPosition(const QVector3D &position) override;
    virtual int getPosition(QVector3D *position) override;
    virtual Qn::PtzCapabilities getCapabilities() override;
    //virtual const QnPtzSpaceMapper *getSpaceMapper() override;

    // TODO: #Elric need to implement this one properly.
    void setFlipped(bool horizontal, bool vertical);
    void getFlipped(bool *horizontal, bool *vertical);

    QString getPtzConfigurationToken();
    void setMediaProfileToken(const QString& value);

private:
    double normalizeSpeed(qreal inputVelocity, const QPair<qreal, qreal>& nativeCoeff, qreal userCoeff);
    int stopMoveInternal();

private:
    QnPlOnvifResourcePtr m_resource;
    Qn::PtzCapabilities m_ptzCapabilities;
    QString m_mediaProfile;
    QString m_ptzConfigurationToken;
    QPair<qreal, qreal> m_xNativeVelocityCoeff; // first for positive value, second for negative
    QPair<qreal, qreal> m_yNativeVelocityCoeff;
    QPair<qreal, qreal> m_zoomNativeVelocityCoeff;
    mutable QMutex m_mutex;

    bool m_verticalFlipped, m_horizontalFlipped;
};

#endif //ENABLE_ONVIF

#endif // QN_ONVIF_PTZ_CONTROLLER_H
