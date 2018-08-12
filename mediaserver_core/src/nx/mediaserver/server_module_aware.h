#pragma once

class QnMediaServerModule;
class QnCommonModule;
class QnResourcePool;
class QnResourcePropertyDictionary;
class QnCameraHistoryPool;
class QnServerDb;
class QnResourceAccessSubjectsCache;
class QnUserRolesManager;

namespace nx::vms::event { class RuleManager; }

namespace nx {
namespace mediaserver {

class Settings;

class ServerModuleAware
{
public:
    ServerModuleAware(QObject* parent);
    ServerModuleAware(QnMediaServerModule* serverModule);

    QnMediaServerModule* serverModule() const;

    QnCommonModule* commonModule() const;

    QnResourcePool* resourcePool() const;
    QnResourcePropertyDictionary* propertyDictionary() const;
    QnCameraHistoryPool* cameraHistoryPool() const;
    const nx::mediaserver::Settings& settings() const;
    nx::vms::event::RuleManager* eventRuleManager() const;
    QnServerDb* serverDb() const;
    QnResourceAccessSubjectsCache* resourceAccessSubjectsCache() const;
    QnUserRolesManager* userRolesManager() const;
private:
    QnMediaServerModule* m_serverModule;

};

} // namespace mediaserver
} // namespace nx
