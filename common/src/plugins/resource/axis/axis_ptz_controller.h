#ifndef QN_AXIS_PTZ_CONTROLLER_H
#define QN_AXIS_PTZ_CONTROLLER_H

#ifdef ENABLE_AXIS

#include <QtCore/QHash>

#include <core/ptz/basic_ptz_controller.h>
#include <utils/math/functors.h>

class CLSimpleHTTPClient;
class QnAxisParameterMap;

class QnAxisPtzController: public QnBasicPtzController {
    Q_OBJECT
    typedef QnBasicPtzController base_type;

public:
    QnAxisPtzController(const QnPlAxisResourcePtr &resource);
    virtual ~QnAxisPtzController();

    virtual Qn::PtzCapabilities getCapabilities() override;

    virtual bool continuousMove(const QVector3D &speed) override;
    virtual bool absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) override;
    
    virtual bool getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) override;
    virtual bool getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits *limits) override;
    virtual bool getFlip(Qt::Orientations *flip) override;

private:
    void updateState();
    void updateState(const QnAxisParameterMap &params);

    CLSimpleHTTPClient *newHttpClient() const;
    bool queryInternal(const QString &request, QByteArray *body = NULL);
    bool query(const QString &request, QByteArray *body = NULL);
    bool query(const QString &request, QnAxisParameterMap *params, QByteArray *body = NULL);
    bool query(const QString &request, int restries, QnAxisParameterMap *params, QByteArray *body = NULL);
    int channel();
    
    QByteArray getCookie() const;
    void setCookie(const QByteArray &cookie);

private:
    QnPlAxisResourcePtr m_resource;
    Qn::PtzCapabilities m_capabilities;
    Qt::Orientations m_flip;
    QnPtzLimits m_limits;
    QnLinearFunction m_35mmEquivToCameraZoom, m_cameraTo35mmEquivZoom;
    QVector3D m_maxDeviceSpeed;

    mutable QMutex m_mutex;
    QByteArray m_cookie;
};

#endif // #ifdef ENABLE_AXIS
#endif // QN_AXIS_PTZ_CONTROLLER_H
