#include "new_system_flag_watcher.h"

#include <api/global_settings.h>
#include <api/app_server_connection.h>

#include <nx_ec/managers/abstract_server_manager.h>
#include <nx_ec/data/api_conversion_functions.h>

#include <common/common_module.h>
#include <common/common_globals.h>

#include <core/resource/media_server_resource.h>

QnNewSystemServerFlagWatcher::QnNewSystemServerFlagWatcher(QObject* parent):
    base_type(parent),
    QnCommonModuleAware(parent)
{
    connect(globalSettings(), &QnGlobalSettings::initialized, this, &QnNewSystemServerFlagWatcher::update);
    connect(globalSettings(), &QnGlobalSettings::localSystemIdChanged, this, &QnNewSystemServerFlagWatcher::update);
}

void QnNewSystemServerFlagWatcher::update()
{
    QnMutexLocker lock(& m_mutex);

    auto server = commonModule()->currentServer();
    NX_ASSERT(server);
    if (!server)
        return;

    auto serverFlags = server->getServerFlags();

    if (qnGlobalSettings->isNewSystem())
        serverFlags |= nx::vms::api::SF_NewSystem;
    else
        serverFlags &= ~nx::vms::api::SF_NewSystem;

    if (serverFlags != server->getServerFlags())
    {
        server->setServerFlags(serverFlags);
        ec2::AbstractECConnectionPtr ec2Connection = QnAppServerConnectionFactory::ec2Connection();

        nx::vms::api::MediaServerData apiServer;
        ec2::fromResourceToApi(server, apiServer);
        ec2Connection->getMediaServerManager(Qn::kSystemAccess)->save(apiServer, this, [] {});
    }
}
