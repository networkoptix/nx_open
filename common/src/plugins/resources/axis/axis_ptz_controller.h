#ifndef QN_AXIS_PTZ_CONTROLLER_H
#define QN_AXIS_PTZ_CONTROLLER_H

#include <QtCore/QHash>

#include <core/resource/interface/abstract_ptz_controller.h>

class CLSimpleHTTPClient;
class QnAxisParameterMap;

class QnAxisPtzController: public QnAbstractPtzController {
    Q_OBJECT;
public:
    QnAxisPtzController(QnPlAxisResource* resource);
    virtual ~QnAxisPtzController();

    virtual Qn::PtzCapabilities getCapabilities() override;

    virtual int startMove(qreal panSpeed, qreal tiltSpeed, qreal zoomSpeed) override;
    virtual int stopMove() override;

    virtual int setPhysicalPosition(qreal pan, qreal tilt, qreal zoom) override;
    virtual int getPhysicalPosition(qreal *pan, qreal *tilt, qreal *zoom) const override;

    virtual int setLogicalPosition(qreal pan, qreal tilt, qreal fov) override;
    virtual int getLogicalPosition(qreal *pan, qreal *tilt, qreal *fov) const override;
    virtual int getLogicalLimits(QnPtzLimits *limits) override;

    virtual int updateState() override;


    virtual int startMove(qreal xVelocity, qreal yVelocity, qreal zoomVelocity) override;
    virtual int moveTo(qreal xPos, qreal yPos, qreal zoomPos) override;
    virtual int getPosition(qreal *xPos, qreal *yPos, qreal *zoomPos) override;
    virtual int stopMove() override;
    virtual Qn::PtzCapabilities getCapabilities() override;
    virtual const QnPtzSpaceMapper *getSpaceMapper() override;

private:
    void updateState(const QnAxisParameterMap &params);

    CLSimpleHTTPClient *newHttpClient() const;
    bool query(const QString &request, QByteArray *body = NULL) const;
    bool query(const QString &request, QnAxisParameterMap *params) const;

private:
    QnPlAxisResource *m_resource;
    Qn::PtzCapabilities m_capabilities;
    QnPtzSpaceMapper *m_spaceMapper;
};


#endif // QN_AXIS_PTZ_CONTROLLER_H
