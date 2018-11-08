#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class StartupActionsHandler: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit StartupActionsHandler(QObject* parent = nullptr);
    virtual ~StartupActionsHandler() override;

private:
    void handleUserLoggedIn(const QnUserResourcePtr& user);
    void submitDelayedDrops();

    void handleStartupParameters();
    void handleInstantDrops(const QnResourceList& resources);
    void handleAcsModeResources(const QnResourceList& resources, const qint64 timeStampMs);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
