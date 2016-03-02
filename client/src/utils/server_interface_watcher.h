#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <utils/common/uuid.h>
#include "network/module_information.h"

//TODO: #dklychkov move the watcher to the network folder, as the router and module finder
class QnServerInterfaceWatcher : public QObject {
    Q_OBJECT
public:
    explicit QnServerInterfaceWatcher(QObject *parent = 0);

private slots:
    void at_connectionChanged(const QnModuleInformation &moduleInformation);
    void at_resourcePool_statusChanged(const QnResourcePtr &resource);
    void at_initialResourcesReceived();

private:
    void updatePriaryInterface(const QnMediaServerResourcePtr &server);
};
