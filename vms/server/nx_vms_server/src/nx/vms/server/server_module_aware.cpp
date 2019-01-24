#include "server_module_aware.h"

#include <media_server/media_server_module.h>

namespace nx {
namespace vms::server {


ServerModuleAware::ServerModuleAware(QnMediaServerModule* serverModule):
    m_serverModule(serverModule)
{
}

ServerModuleAware::ServerModuleAware(QObject* parent)
{
    for (; parent; parent = parent->parent())
    {
        ServerModuleAware* moduleAware = dynamic_cast<ServerModuleAware*>(parent);
        if (moduleAware != nullptr)
        {
            m_serverModule = moduleAware->serverModule();
            NX_ASSERT(m_serverModule, "Invalid context");
            break;
        }

        m_serverModule = dynamic_cast<QnMediaServerModule*>(parent);
        if (m_serverModule)
            break;
    }
    NX_ASSERT(m_serverModule);
}

QnMediaServerModule* ServerModuleAware::serverModule() const
{
    return m_serverModule;
}

QnResourcePool* ServerModuleAware::resourcePool() const
{
    return m_serverModule ? m_serverModule->resourcePool() : nullptr;
}

QnResourcePropertyDictionary* ServerModuleAware::propertyDictionary() const
{
    return m_serverModule ? m_serverModule->propertyDictionary() : nullptr;
}

QnCameraHistoryPool* ServerModuleAware::cameraHistoryPool() const
{
    return m_serverModule ? m_serverModule->cameraHistoryPool() : nullptr;
}

const nx::vms::server::Settings& ServerModuleAware::settings() const
{
    return m_serverModule->settings();
}

nx::vms::event::RuleManager* ServerModuleAware::eventRuleManager() const
{
    return m_serverModule->commonModule()->eventRuleManager();
}

QnServerDb* ServerModuleAware::serverDb() const
{
    return m_serverModule->serverDb();
}

QnResourceAccessSubjectsCache* ServerModuleAware::resourceAccessSubjectsCache() const
{
    return m_serverModule->commonModule()->resourceAccessSubjectsCache();
}

QnUserRolesManager* ServerModuleAware::userRolesManager() const
{
    return m_serverModule->commonModule()->userRolesManager();
}

QnAudioStreamerPool* ServerModuleAware::audioStreamPool() const
{
    return m_serverModule->audioStreamPool();
}

QnStorageDbPool* ServerModuleAware::storageDbPool() const
{
    return m_serverModule->storageDbPool();
}

QnRecordingManager* ServerModuleAware::recordingManager() const
{
    return m_serverModule->recordingManager();
}

nx::vms::server::event::EventMessageBus* ServerModuleAware::eventMessageBus() const
{
    return m_serverModule->eventMessageBus();
}

QnVideoCameraPool* ServerModuleAware::videoCameraPool() const
{
    return m_serverModule->videoCameraPool();
}

RootFileSystem* ServerModuleAware::rootFileSystem() const
{
    return m_serverModule->rootFileSystem();
}

QnUuid ServerModuleAware::moduleGUID() const
{
    return m_serverModule->commonModule()->moduleGUID();
}

std::shared_ptr<ec2::AbstractECConnection> ServerModuleAware::ec2Connection() const
{
    return m_serverModule->commonModule()->ec2Connection();
}

QnGlobalSettings* ServerModuleAware::globalSettings() const
{
    return m_serverModule->commonModule()->globalSettings();
}

QnRuntimeInfoManager* ServerModuleAware::runtimeInfoManager() const
{
    return m_serverModule->commonModule()->runtimeInfoManager();
}

QnCameraUserAttributePool* ServerModuleAware::cameraUserAttributesPool() const
{
    return m_serverModule->commonModule()->cameraUserAttributesPool();
}

QnAuditManager* ServerModuleAware::auditManager() const
{
    return m_serverModule->commonModule()->auditManager();
}

} // namespace vms::server
} // namespace nx
