#ifndef QN_ACTIVITY_PTZ_CONTROLLER_H
#define QN_ACTIVITY_PTZ_CONTROLLER_H

#include "proxy_ptz_controller.h"

class QnActivityPtzController: public QnProxyPtzController {
    Q_OBJECT
    typedef QnProxyPtzController base_type;
public:
    QnActivityPtzController(bool isLocal, const QnPtzControllerPtr &baseController);
    virtual ~QnActivityPtzController();

    virtual bool activatePreset(const QString &presetId, qreal speed) override;
    virtual bool activateTour(const QString &tourId) override;

protected:
    virtual void baseFinished(Qn::PtzCommand command, const QVariant &data) override;

private:
    bool m_isLocal;
};

#endif


#endif // QN_ACTIVITY_PTZ_CONTROLLER_H
