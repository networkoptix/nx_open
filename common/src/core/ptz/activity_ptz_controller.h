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

    virtual bool removePreset(const QString &presetId) override;
    virtual bool activatePreset(const QString &presetId, qreal speed) override;

    virtual bool removeTour(const QString &tourId) override;
    virtual bool activateTour(const QString &tourId) override;

    virtual bool getActiveObject(QnPtzObject *activeObject) override;

    virtual bool getData(Qn::PtzDataFields query, QnPtzData *data) override;

private:
    const Mode m_mode;
    QnResourcePropertyAdaptor<QnPtzObject> *m_adaptor;
};

#endif // QN_ACTIVITY_PTZ_CONTROLLER_H
