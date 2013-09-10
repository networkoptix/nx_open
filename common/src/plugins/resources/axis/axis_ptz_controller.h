#ifndef QN_AXIS_PTZ_CONTROLLER_H
#define QN_AXIS_PTZ_CONTROLLER_H

#include <QtCore/QHash>

#include <core/ptz/abstract_ptz_controller.h>

class CLSimpleHTTPClient;
class QnAxisParameterMap;

class QnAxisPtzController: public QnAbstractPtzController {
    Q_OBJECT;
public:
    QnAxisPtzController(QnPlAxisResource* resource);
    virtual ~QnAxisPtzController();

    virtual Qn::PtzCapabilities getCapabilities() override;

    virtual int startMove(const QVector3D &speed) override;
    virtual int setPosition(const QVector3D &position) override;
    virtual int getPosition(QVector3D *position) override;
    virtual int stopMove() override;
    //virtual const QnPtzSpaceMapper *getSpaceMapper() override;

private:
    void updateState(const QnAxisParameterMap &params);

    CLSimpleHTTPClient *newHttpClient() const;
    bool query(const QString &request, QByteArray *body = NULL) const;
    bool query(const QString &request, QnAxisParameterMap *params) const;

private:
    QnPlAxisResource *m_resource;
    Qn::PtzCapabilities m_capabilities;
    //QnPtzSpaceMapper *m_spaceMapper;
};


#endif // QN_AXIS_PTZ_CONTROLLER_H
