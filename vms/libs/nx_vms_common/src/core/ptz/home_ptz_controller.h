// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_HOME_PTZ_CONTROLLER_H
#define QN_HOME_PTZ_CONTROLLER_H

#include <nx/utils/thread/mutex.h>

#include "proxy_ptz_controller.h"

template<class T>
class QnResourcePropertyAdaptor;

class QnHomePtzExecutor;

class NX_VMS_COMMON_API QnHomePtzController: public QnProxyPtzController
{
    using base_type = QnProxyPtzController;

public:
    QnHomePtzController(const QnPtzControllerPtr& baseController, QThread* executorThread);
    virtual ~QnHomePtzController();

    static bool extends(Ptz::Capabilities capabilities);

    virtual Ptz::Capabilities getCapabilities(const Options& options) const override;

    virtual bool continuousMove(
        const Vector& speed,
        const Options& options) override;

    virtual bool absoluteMove(
        CoordinateSpace space,
        const Vector& position,
        qreal speed,
        const Options& options) override;

    virtual bool viewportMove(
        qreal aspectRatio,
        const QRectF& viewport,
        qreal speed,
        const Options& options) override;

    virtual bool activatePreset(const QString& presetId, qreal speed) override;
    virtual bool activateTour(const QString& tourId) override;

    virtual bool updateHomeObject(const QnPtzObject& homePosition) override;
    virtual bool getHomeObject(QnPtzObject* homePosition) const override;

protected:
    virtual void restartExecutor();

private:
    void at_adaptor_valueChanged(const QString& key, const QString& value);

protected:
    QnResourcePropertyAdaptor<QnPtzObject>* m_adaptor;
    QnHomePtzExecutor* m_executor;
};


#endif // QN_HOME_PTZ_CONTROLLER_H
