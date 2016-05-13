#include "new_system_flag_watcher.h"

#include <boost/algorithm/cxx11/none_of.hpp>

#include <api/global_settings.h>
#include <api/app_server_connection.h>

#include <nx_ec/managers/abstract_server_manager.h>
#include <nx_ec/data/api_conversion_functions.h>

#include <common/common_module.h>
#include <common/common_globals.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>

#include <nx/utils/log/log.h>
#include "serverutil.h"

namespace
{
    const QString kDefaultOwnerName(lit("admin"));
    const QString kDefaultOwnerPassword(lit("admin"));
}

QnNewSystemServerFlagWatcher::QnNewSystemServerFlagWatcher(QObject* parent /*= nullptr*/) :
    base_type(parent),
    m_server()
{
    connect(qnGlobalSettings, &QnGlobalSettings::initialized,       this, &QnNewSystemServerFlagWatcher::update);
    connect(qnGlobalSettings, &QnGlobalSettings::newSystemChanged,  this, &QnNewSystemServerFlagWatcher::update);

    connect(qnResPool, &QnResourcePool::resourceAdded,              this, &QnNewSystemServerFlagWatcher::at_resourcePool_resourceAdded);
    for (const QnResourcePtr &resource : qnResPool->getResources())
        at_resourcePool_resourceAdded(resource);
}

QnNewSystemServerFlagWatcher::~QnNewSystemServerFlagWatcher()
{
}

void QnNewSystemServerFlagWatcher::update()
{
    if (!m_server || !qnGlobalSettings->isInitialized())
        return;

    auto newSystemMustBeSet = [this]
    {
        if (!qnGlobalSettings->cloudSystemID().isEmpty())
            return false;

        return boost::algorithm::none_of(qnResPool->getResources<QnUserResource>(), [](const QnUserResourcePtr& user)
        {
            return user->isOwner() && user->isEnabled();
        });
    };

    auto newSystemMustBeCleaned = [this]
    {
        nx::SystemName systemName;
        systemName.loadFromConfig();
        if (systemName.isDefault())
            return false;

        if (!qnGlobalSettings->cloudSystemID().isEmpty())
            return true;

        auto owners = qnResPool->getResources<QnUserResource>().filtered([](const QnUserResourcePtr& user)
        {
            return user->isOwner() && user->isEnabled();
        });

        /* Check if there are other owners but admin. */
        if (owners.size() != 1)
            return true;

        QnUserResourcePtr owner = owners.first();
        if (owner->getName() != kDefaultOwnerName)
            return true;

        /* Check if admin's password was changed. */
        return !owner->checkPassword(kDefaultOwnerPassword);
    };

    if (qnGlobalSettings->isNewSystem())
    {
        if (newSystemMustBeCleaned())
            qnGlobalSettings->setNewSystem(false);
    }
    else
    {
        if (newSystemMustBeSet())
            qnGlobalSettings->setNewSystem(true);
    }
    qnGlobalSettings->synchronizeNow();
    qnCommon->updateModuleInformation();

    Qn::ServerFlags serverFlags = m_server->getServerFlags();

    if (qnGlobalSettings->isNewSystem())
        serverFlags |= Qn::SF_NewSystem;
    else
        serverFlags &= ~Qn::SF_NewSystem;

    //TODO: #GDM code duplication
    if (serverFlags != m_server->getServerFlags())
    {
        m_server->setServerFlags(serverFlags);
        ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::getConnection2();

        ec2::ApiMediaServerData apiServer;
        fromResourceToApi(m_server, apiServer);
        ec2Connection->getMediaServerManager()->save(apiServer, this, [] {});
    }

}

void QnNewSystemServerFlagWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource)
{
    if (m_server)
        return;

    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    NX_ASSERT(!qnCommon->moduleGUID().isNull(), Q_FUNC_INFO, "Module GUID must be set before server added to pool");
    if (qnCommon->moduleGUID().isNull())
        return;

    if (server->getId() != qnCommon->moduleGUID())
        return;

    m_server = server;
    update();

    disconnect(qnResPool, nullptr, this, nullptr);
}
