#ifndef QN_PTZ_CONTROLLER_POOL_H
#define QN_PTZ_CONTROLLER_POOL_H

#include <QtCore/QObject>

#include <utils/common/singleton.h>
#include <utils/common/connective.h>

#include <core/resource/resource_fwd.h>
#include <core/ptz/ptz_fwd.h>


class QnPtzControllerPool: public Connective<QObject>, public Singleton<QnPtzControllerPool> {
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    enum ControllerRole {
        DefaultController,
        DeviceController
    };

    QnPtzControllerPool(QObject *parent = NULL);
    virtual ~QnPtzControllerPool();

    QnPtzControllerPtr controller(const QnResourcePtr &resource, ControllerRole role = DefaultController) const;

private slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);
    void at_resource_initAsyncFinished(const QnResourcePtr &resource);

private:
    struct PtzData {
        QnPtzControllerPtr deviceController;
        QnPtzControllerPtr logicalController;
        QnPtzControllerPtr relativeController;
        QnPtzControllerPtr workaroundController;

        QnPtzControllerPtr defaultController;
    };

    QHash<QnResourcePtr, PtzData> m_dataByResource;
};

#define qnPtzPool (QnPtzControllerPool::instance())


#endif // QN_PTZ_CONTROLLER_POOL_H
