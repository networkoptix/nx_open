#pragma once

#include <core/ptz/ptz_object.h>
#include <core/ptz/ptz_controller_pool.h>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

class QnServerPtzControllerPool: public QnPtzControllerPool
{
    Q_OBJECT
    typedef QnPtzControllerPool base_type;

public:
    QnServerPtzControllerPool(QObject *parent = NULL);
    ~QnServerPtzControllerPool();

protected:
    virtual void registerResource(const QnResourcePtr &resource) override;
    virtual void unregisterResource(const QnResourcePtr &resource) override;
    virtual QnPtzControllerPtr createController(const QnResourcePtr &resource) const override;

private slots:
    void at_cameraPropertyChanged(const QnResourcePtr &resource, const QString& key);

    void at_controllerAboutToBeChanged(const QnResourcePtr &resource);
    void at_controllerChanged(const QnResourcePtr &resource);

private:
    mutable QnMutex m_mutex;
    QHash<QnResourcePtr, QnPtzObject> activeObjectByResource;
};
