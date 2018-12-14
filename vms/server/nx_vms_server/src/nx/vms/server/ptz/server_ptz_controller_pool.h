#pragma once

#include <core/ptz/ptz_object.h>
#include <core/ptz/ptz_controller_pool.h>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

namespace nx {
namespace vms::server {
namespace ptz {

class ServerPtzControllerPool: public QnPtzControllerPool
{
    Q_OBJECT
    typedef QnPtzControllerPool base_type;

public:
    ServerPtzControllerPool(QObject *parent = NULL);
    virtual ~ServerPtzControllerPool();

protected:
    virtual void registerResource(const QnResourcePtr &resource) override;
    virtual void unregisterResource(const QnResourcePtr &resource) override;
    virtual QnPtzControllerPtr createController(const QnResourcePtr &resource) const override;

private slots:
    void at_ptzConfigurationChanged(const QnResourcePtr &resource);
    void at_controllerAboutToBeChanged(const QnResourcePtr &resource);
    void at_controllerChanged(const QnResourcePtr &resource);

private:
    mutable QnMutex m_mutex;
    QHash<QnResourcePtr, QnPtzObject> activeObjectByResource;
};

} // namespace ptz
} // namespace vms::server
} // namespace nx
