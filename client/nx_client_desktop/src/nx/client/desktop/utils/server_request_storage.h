#pragma once

#include <QtCore/QSet>

#include <core/resource/resource_fwd.h>
#include <api/server_rest_connection_fwd.h>

namespace nx {
namespace client {
namespace desktop {

class ServerRequestStorage
{
public:
    ServerRequestStorage(const QnMediaServerResourcePtr& server);
    ~ServerRequestStorage();

    void storeHandle(rest::Handle handle);
    void releaseHandle(rest::Handle handle);
    void cancelAllRequests();

private:
    QnMediaServerResourcePtr m_server;
    QSet<rest::Handle> m_handles;
};

} // namespace desktop
} // namespace client
} // namespace nx

