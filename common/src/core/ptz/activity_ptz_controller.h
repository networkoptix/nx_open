#ifndef QN_ACTIVITY_PTZ_CONTROLLER_H
#define QN_ACTIVITY_PTZ_CONTROLLER_H

#include "proxy_ptz_controller.h"

template<class T>
class QnResourcePropertyAdaptor;

class QnActivityPtzController: public QnProxyPtzController {
    Q_OBJECT
    typedef QnProxyPtzController base_type;

public:
    QnActivityPtzController(bool isLocal, const QnPtzControllerPtr &baseController);
    virtual ~QnActivityPtzController();

    virtual bool activatePreset(const QString &presetId, qreal speed) override;
    virtual bool activateTour(const QString &tourId) override;

    virtual bool getActiveObject(QnPtzObject *activeObject) override;

private:
    void setActiveObject(const QnPtzObject &activeObject);

    Q_SLOT void at_adaptor_valueChanged();

private:
    bool m_isLocal;
    QnResourcePropertyAdaptor<QnPtzObject> *m_adaptor;
    QnPtzObject m_activeObject;
};

#endif // QN_ACTIVITY_PTZ_CONTROLLER_H
