#pragma once

class QnMediaServerModule;
class QnCommonModule;
class QnResourcePool;
class QnResourcePropertyDictionary;
class QnCameraHistoryPool;

namespace nx {
namespace mediaserver {

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

private:
    QnMediaServerModule* m_serverModule;

};

} // namespace mediaserver
} // namespace nx
