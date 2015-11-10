#include "desktop_client_message_processor.h"

#include <core/resource_management/incompatible_server_watcher.h>

QnDesktopClientMessageProcessor::QnDesktopClientMessageProcessor() :
    base_type(),
    m_incompatibleServerWatcher(new QnIncompatibleServerWatcher(this))
{
    connect(this, &QnClientMessageProcessor::connectionClosed, m_incompatibleServerWatcher, &QnIncompatibleServerWatcher::stop);
}

QnIncompatibleServerWatcher *QnDesktopClientMessageProcessor::incompatibleServerWatcher() const {
    return m_incompatibleServerWatcher;
}

void QnDesktopClientMessageProcessor::connectToConnection(const ec2::AbstractECConnectionPtr &connection) {
    base_type::connectToConnection(connection);
    connect(connection->getMiscManager(), &ec2::AbstractMiscManager::gotInitialModules,
            this, &QnDesktopClientMessageProcessor::at_gotInitialModules);
}

void QnDesktopClientMessageProcessor::disconnectFromConnection(const ec2::AbstractECConnectionPtr &connection) {
    base_type::disconnectFromConnection(connection);
    connection->getMiscManager()->disconnect(this);
}

void QnDesktopClientMessageProcessor::at_gotInitialModules(const QList<QnModuleInformationWithAddresses> &modules) {
    m_incompatibleServerWatcher->start();
    m_incompatibleServerWatcher->createModules(modules);
}

