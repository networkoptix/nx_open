#pragma once

#include <QtCore/QObject>

#include <client_core/connection_context_aware.h>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include "network/module_information.h"

//TODO: #dklychkov move the watcher to the network folder, as the router and module finder
class QnServerInterfaceWatcher : public QObject, public QnConnectionContextAware
{
    Q_OBJECT
public:
    explicit QnServerInterfaceWatcher(QObject* parent = nullptr);

private slots:
    void at_connectionChanged(const QnModuleInformation& moduleInformation);
    void at_resourcePool_statusChanged(const QnResourcePtr& resource);
    void at_resourcePool_resourceAdded(const QnResourcePtr& resource);

private:
    void updatePrimaryInterface(const QnMediaServerResourcePtr& server);
};
