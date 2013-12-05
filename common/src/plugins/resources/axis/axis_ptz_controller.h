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
    virtual bool getFlip(Qt::Orientations *flip) override;
    virtual bool absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position) override;
    virtual bool getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) override;
    virtual bool getLimits(QnPtzLimits *limits) override;

private:
    void updateState();
    void updateState(const QnAxisParameterMap &params);

    CLSimpleHTTPClient *newHttpClient() const;
    bool query(const QString &request, QByteArray *body = NULL) const;
    bool query(const QString &request, QnAxisParameterMap *params) const;

private:
    QnPlAxisResourcePtr m_resource;
    Qn::PtzCapabilities m_capabilities;
    Qt::Orientations m_flip;
    QnPtzLimits m_limits;
    QnLinearFunction m_logicalToCameraZoom, m_cameraToLogicalZoom;
};

#endif // #ifdef ENABLE_AXIS
#endif // QN_AXIS_PTZ_CONTROLLER_H
