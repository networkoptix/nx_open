#pragma once

class QnMediaServerModule;
class QnCommonModule;

namespace nx {
namespace mediaserver {

class ServerModuleAware
{
public:
    ServerModuleAware(QnMediaServerModule* serverModule);

    QnMediaServerModule* serverModule() const;

    QnCommonModule* commonModule() const;

private:
    QnMediaServerModule* m_serverModule;

};


} // namespace mediaserver
} // namespace nx
