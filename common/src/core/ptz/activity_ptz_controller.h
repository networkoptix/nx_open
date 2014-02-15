#ifndef QN_ACTIVITY_PTZ_CONTROLLER_H
#define QN_ACTIVITY_PTZ_CONTROLLER_H

#include "proxy_ptz_controller.h"

template<class T>
class QnResourcePropertyAdaptor;

class QnActivityPtzController: public QnProxyPtzController {
    Q_OBJECT
    typedef QnProxyPtzController base_type;

public:
    enum Mode {
        Local,
        Client,
        Server
    };

    QnActivityPtzController(Mode mode, const QnPtzControllerPtr &baseController);
    virtual ~QnActivityPtzController();

    static bool extends(Qn::PtzCapabilities capabilities);

    virtual Qn::PtzCapabilities getCapabilities() override;

    virtual bool continuousMove(const QVector3D &speed) override;
    virtual bool absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) override;
    virtual bool viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) override;

    virtual bool activatePreset(const QString &presetId, qreal speed) override;
    virtual bool activateTour(const QString &tourId) override;

    virtual bool getActiveObject(QnPtzObject *activeObject) override;

private:
    void setActiveObject(const QnPtzObject &activeObject);

private:
    Mode m_mode;
    QnResourcePropertyAdaptor<QnPtzObject> *m_adaptor;
    QnPtzObject m_activeObject;
};

#endif // QN_ACTIVITY_PTZ_CONTROLLER_H
