#ifndef QN_SERVER_PTZ_CONTROLLER_POOL_H
#define QN_SERVER_PTZ_CONTROLLER_POOL_H

#include <core/ptz/ptz_controller_pool.h>

class QnServerPtzControllerPool: public QnPtzControllerPool {
    Q_OBJECT
    typedef QnPtzControllerPool base_type;

public:
    QnServerPtzControllerPool(QObject *parent = NULL): base_type(parent) {}

protected:
    virtual void registerResource(const QnResourcePtr &resource) override;
    virtual void unregisterResource(const QnResourcePtr &resource) override;
    virtual QnPtzControllerPtr createController(const QnResourcePtr &resource) const override;
};

#endif // QN_SERVER_PTZ_CONTROLLER_POOL_H
