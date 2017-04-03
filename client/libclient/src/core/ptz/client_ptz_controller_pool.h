#pragma once

#include <core/ptz/ptz_controller_pool.h>

class QnClientPtzControllerPool: public QnPtzControllerPool
{
    Q_OBJECT
    typedef QnPtzControllerPool base_type;

public:
    QnClientPtzControllerPool(QObject *parent = NULL);

protected:
    virtual void registerResource(const QnResourcePtr &resource) override;
    virtual void unregisterResource(const QnResourcePtr &resource) override;
    virtual QnPtzControllerPtr createController(const QnResourcePtr &resource) const override;

private:
    void cacheCameraPresets(const QnResourcePtr &resource);
};
