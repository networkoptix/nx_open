// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/singleton.h>
#include <utils/common/connective.h>

#include <core/resource/resource_fwd.h>
#include <core/ptz/ptz_fwd.h>
#include <common/common_module_aware.h>

class QThread;
class QThreadPool;

class QnPtzControllerPoolPrivate;

class NX_VMS_COMMON_API QnPtzControllerPool:
    public Connective<QObject>,
    public /*mixin*/ QnCommonModuleAware
{
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    enum ControllerConstructionMode
    {
        NormalControllerConstruction,
        ThreadedControllerConstruction
    };

    QnPtzControllerPool(QObject* parent);
    virtual ~QnPtzControllerPool();

    QThread *executorThread() const;
    QThreadPool *commandThreadPool() const;

    ControllerConstructionMode constructionMode() const;
    void setConstructionMode(ControllerConstructionMode mode);

    QnPtzControllerPtr controller(const QnResourcePtr &resource) const;

    virtual void init();
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

    virtual void deinitialize();

private:
    friend class QnPtzControllerCreationCommand;
    friend class QnPtzControllerPoolPrivate;
    QScopedPointer<QnPtzControllerPoolPrivate> d;
};

