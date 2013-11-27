#ifndef QN_AXIS_PTZ_CONTROLLER_H
#define QN_AXIS_PTZ_CONTROLLER_H

#include <QtCore/QHash>

#include <core/ptz/abstract_ptz_controller.h>
#include <utils/math/functors.h>

class CLSimpleHTTPClient;
class QnAxisParameterMap;

class QnAxisPtzController: public QnAbstractPtzController {
    Q_OBJECT;
public:
    QnAxisPtzController(const QnPlAxisResourcePtr &resource);
    virtual ~QnAxisPtzController();

    virtual Qn::PtzCapabilities getCapabilities() override;

    virtual int continuousMove(const QVector3D &speed) override;
    virtual int getFlip(Qt::Orientations *flip) override;
    virtual int absoluteMove(const QVector3D &position) override;
    virtual int getPosition(QVector3D *position) override;
    virtual int getLimits(QnPtzLimits *limits) override;
    virtual int relativeMove(qreal aspectRatio, const QRectF &viewport) override;

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


#endif // QN_AXIS_PTZ_CONTROLLER_H
