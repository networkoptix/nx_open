#ifndef QN_CLIENT_PTZ_CONTROLLER_POOL_H
#define QN_CLIENT_PTZ_CONTROLLER_POOL_H

#include <core/ptz/ptz_controller_pool.h>

class QnClientPtzControllerPool: public QnPtzControllerPool {
    Q_OBJECT
    typedef QnPtzControllerPool base_type;

public:
    QnClientPtzControllerPool(QObject *parent = NULL): base_type(parent) {}

protected:
    virtual void registerResource(const QnResourcePtr &resource) override;
    virtual void unregisterResource(const QnResourcePtr &resource) override;
    virtual QnPtzControllerPtr createController(const QnResourcePtr &resource) override;
};

#endif // QN_CLIENT_PTZ_CONTROLLER_POOL_H
