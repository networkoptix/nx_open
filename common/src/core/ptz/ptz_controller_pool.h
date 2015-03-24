#ifndef QN_PTZ_CONTROLLER_POOL_H
#define QN_PTZ_CONTROLLER_POOL_H

#include <QtCore/QObject>

#include <utils/common/singleton.h>
#include <utils/common/connective.h>

#include <core/resource/resource_fwd.h>
#include <core/ptz/ptz_fwd.h>

class QThread;
class QThreadPool;

class QnPtzControllerPoolPrivate;

class QnPtzControllerPool: public Connective<QObject>, public Singleton<QnPtzControllerPool> {
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    enum ControllerConstructionMode {
        NormalControllerConstruction,
        ThreadedControllerConstruction
    };

    QnPtzControllerPool(QObject *parent = NULL);
    virtual ~QnPtzControllerPool();

    QThread *executorThread() const;
    QThreadPool *commandThreadPool() const;

    ControllerConstructionMode constructionMode() const;
    void setConstructionMode(ControllerConstructionMode mode);

    QnPtzControllerPtr controller(const QnResourcePtr &resource) const;

signals:
    void controllerAboutToBeChanged(const QnResourcePtr &resource);
    void controllerChanged(const QnResourcePtr &resource);

protected:
    /**
     * \param resource                  Resource to create PTZ controller for.
     * \returns                         Newly created PTZ controller for the given resource.
     * \note                            This function is supposed to be thread-safe.
     */
    Q_SLOT virtual QnPtzControllerPtr createController(const QnResourcePtr &resource) const;

    Q_SLOT virtual void registerResource(const QnResourcePtr &resource);
    Q_SLOT virtual void unregisterResource(const QnResourcePtr &resource);

    Q_SLOT void updateController(const QnResourcePtr &resource);

private:
    friend class QnPtzControllerCreationCommand;
    friend class QnPtzControllerPoolPrivate;
    QScopedPointer<QnPtzControllerPoolPrivate> d;
};

#define qnPtzPool (QnPtzControllerPool::instance())


#endif // QN_PTZ_CONTROLLER_POOL_H
