#include "workbench_server_safemode_watcher.h"

#include <common/common_module.h>
#include <api/common_message_processor.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

QnWorkbenchServerSafemodeWatcher::QnWorkbenchServerSafemodeWatcher(QObject *parent /*= nullptr*/)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
{
    connect(commonModule(), &QnCommonModule::remoteIdChanged, this, &QnWorkbenchServerSafemodeWatcher::updateCurrentServer);
    connect(commonModule(), &QnCommonModule::readOnlyChanged, this, &QnWorkbenchServerSafemodeWatcher::updateServerFlags);

    connect(qnCommonMessageProcessor, &QnCommonMessageProcessor::initialResourcesReceived, this, [this] {
        updateCurrentServer();
        updateServerFlags();
    });

    connect(resourcePool(), &QnResourcePool::resourceAdded, this, [this](const QnResourcePtr &resource) {
        if (m_currentServer || commonModule()->moduleGUID().isNull() || resource->getId() != commonModule()->remoteGUID())
            return;

        m_currentServer = resource.dynamicCast<QnMediaServerResource>();
        updateServerFlags();
    });


    connect(resourcePool(), &QnResourcePool::resourceRemoved, this, [this](const QnResourcePtr &resource) {
        if (!m_currentServer || m_currentServer != resource)
            return;
        m_currentServer.reset();
    });

    updateCurrentServer() ;
    updateServerFlags();
}

void QnWorkbenchServerSafemodeWatcher::updateCurrentServer() {
    m_currentServer = resourcePool()->getResourceById<QnMediaServerResource>(commonModule()->remoteGUID());
}

void QnWorkbenchServerSafemodeWatcher::updateServerFlags() {
    if (!m_currentServer)
        return;

    if (commonModule()->isReadOnly())
        m_currentServer->addFlags(Qn::read_only);
    else
        m_currentServer->removeFlags(Qn::read_only);
}
