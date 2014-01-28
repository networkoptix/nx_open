#ifndef QN_PTZ_CONTROLLER_POOL_H
#define QN_PTZ_CONTROLLER_POOL_H

#include <QtCore/QObject>
#include <QtCore/QMutex>

#include <utils/common/singleton.h>
#include <utils/common/connective.h>

#include <core/resource/resource_fwd.h>
#include <core/ptz/ptz_fwd.h>

class QnPtzControllerPool: public Connective<QObject>, public Singleton<QnPtzControllerPool> {
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    QnPtzControllerPool(QObject *parent = NULL);
    virtual ~QnPtzControllerPool();

    QThread *executorThread() const { return m_executorThread; }

    QnPtzControllerPtr controller(const QnResourcePtr &resource) const;

signals:
    void controllerChanged(const QnResourcePtr &resource);

protected:
    void setController(const QnResourcePtr &resource, const QnPtzControllerPtr &controller);

    Q_SLOT virtual void registerResource(const QnResourcePtr &resource);
    Q_SLOT virtual void unregisterResource(const QnResourcePtr &resource);
    Q_SLOT virtual QnPtzControllerPtr createController(const QnResourcePtr &resource);

    Q_SLOT void updateController(const QnResourcePtr &resource);

private:
    mutable QMutex m_mutex;
    QHash<QnResourcePtr, QnPtzControllerPtr> m_controllerByResource;
    QThread *m_executorThread;
};

#define qnPtzPool (QnPtzControllerPool::instance())


#endif // QN_PTZ_CONTROLLER_POOL_H
