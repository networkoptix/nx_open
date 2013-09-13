#ifndef QN_AXIS_PTZ_CONTROLLER_H
#define QN_AXIS_PTZ_CONTROLLER_H

#include <QtCore/QHash>

#include <core/ptz/abstract_ptz_controller.h>

class CLSimpleHTTPClient;
class QnAxisParameterMap;

class QnAxisPtzController: public QnAbstractPtzController {
    Q_OBJECT;
public:
    QnAxisPtzController(const QnPlAxisResourcePtr &resource);
    virtual ~QnAxisPtzController();

    virtual Qn::PtzCapabilities getCapabilities() override;

    virtual int startMove(const QVector3D &speed) override;
    virtual int stopMove() override;
    virtual int getFlip(Qt::Orientations *flip) override;
    virtual int setPosition(const QVector3D &position) override;
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
};


#endif // QN_AXIS_PTZ_CONTROLLER_H
