#pragma once

class QnMediaServerModule;

namespace nx {
namespace mediaserver {

class ServerModuleAware
{
public:
    ServerModuleAware(QnMediaServerModule* serverModule);

    QnMediaServerModule* serverModule() const;

private:
    QnMediaServerModule* m_serverModule;

};


} // namespace mediaserver
} // namespace nx
