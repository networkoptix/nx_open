#ifndef QN_HOME_PTZ_CONTROLLER_H
#define QN_HOME_PTZ_CONTROLLER_H

#include <nx/utils/thread/mutex.h>

#include "proxy_ptz_controller.h"

template<class T>
class QnResourcePropertyAdaptor;

class QnHomePtzExecutor;

class QnHomePtzController: public QnProxyPtzController
{
    Q_OBJECT
    using base_type = QnProxyPtzController;

public:
    QnHomePtzController(const QnPtzControllerPtr& baseController, QThread* executorThread);
    virtual ~QnHomePtzController();

    static bool extends(Ptz::Capabilities capabilities);

    virtual Ptz::Capabilities getCapabilities() const override;

    virtual bool continuousMove(const QVector3D& speed) override;
    virtual bool absoluteMove(
        Qn::PtzCoordinateSpace space,
        const QVector3D& position,
        qreal speed) override;
    virtual bool viewportMove(qreal aspectRatio, const QRectF& viewport, qreal speed) override;
    virtual bool activatePreset(const QString& presetId, qreal speed) override;
    virtual bool activateTour(const QString& tourId) override;

    virtual bool updateHomeObject(const QnPtzObject& homePosition) override;
    virtual bool getHomeObject(QnPtzObject* homePosition) const override;

protected:
    virtual void restartExecutor();

private:
    void at_adaptor_valueChanged();

public:
    QnResourcePropertyAdaptor<QnPtzObject>* m_adaptor;
    QnHomePtzExecutor* m_executor;
};


#endif // QN_HOME_PTZ_CONTROLLER_H
