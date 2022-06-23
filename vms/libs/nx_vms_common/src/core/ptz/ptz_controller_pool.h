// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/ptz/ptz_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/common/system_context_aware.h>

class QThread;
class QThreadPool;

class NX_VMS_COMMON_API QnPtzControllerPool:
    public QObject,
    public nx::vms::common::SystemContextAware
{
    Q_OBJECT

public:
    enum ControllerConstructionMode
    {
        NormalControllerConstruction,
        ThreadedControllerConstruction
    };

    QnPtzControllerPool(nx::vms::common::SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~QnPtzControllerPool();

    QThread* executorThread() const;
    QThreadPool* commandThreadPool() const;

    ControllerConstructionMode constructionMode() const;
    void setConstructionMode(ControllerConstructionMode mode);

    QnPtzControllerPtr controller(const QnResourcePtr& resource) const;

    virtual void init();

signals:
    void controllerAboutToBeChanged(const QnResourcePtr& resource);
    void controllerChanged(const QnResourcePtr& resource);

protected:
    /**
     * \param resource                  Resource to create PTZ controller for.
     * \returns                         Newly created PTZ controller for the given resource.
     * \note                            This function is supposed to be thread-safe.
     */
    virtual QnPtzControllerPtr createController(const QnResourcePtr& resource) const;

    virtual void registerResource(const QnResourcePtr& resource);
    virtual void unregisterResource(const QnResourcePtr& resource);

    void updateController(const QnResourcePtr& resource);

    virtual void deinitialize();

private:
    friend class QnPtzControllerCreationCommand;
    struct Private;
    nx::utils::ImplPtr<Private> d;
};
