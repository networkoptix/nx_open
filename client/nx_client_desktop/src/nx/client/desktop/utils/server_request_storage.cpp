#include "server_request_storage.h"

#include <api/server_rest_connection.h>
#include <core/resource/media_server_resource.h>

namespace nx {
namespace client {
namespace desktop {

ServerRequestStorage::ServerRequestStorage(const QnMediaServerResourcePtr& server):
    m_server(server)
{
}

ServerRequestStorage::~ServerRequestStorage()
{
    cancelAllRequests();
}

void ServerRequestStorage::storeHandle(rest::Handle handle)
{
    NX_ASSERT(!m_handles.contains(handle));
    m_handles.insert(handle);
}

void ServerRequestStorage::releaseHandle(rest::Handle handle)
{
    NX_ASSERT(m_handles.contains(handle));
    m_handles.remove(handle);
}

void ServerRequestStorage::cancelAllRequests()
{
    for (rest::Handle handle : m_handles)
        m_server->restConnection()->cancelRequest(handle);
    m_handles.clear();
}

} // namespace desktop
} // namespace client
} // namespace nx

