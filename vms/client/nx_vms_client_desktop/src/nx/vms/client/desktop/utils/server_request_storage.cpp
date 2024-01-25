// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_request_storage.h"

#include <api/server_rest_connection.h>

namespace nx::vms::client::desktop {

ServerRequestStorage::ServerRequestStorage()
{
}

ServerRequestStorage::~ServerRequestStorage()
{
    cancelAllRequests();
}

void ServerRequestStorage::storeHandle(rest::Handle handle)
{
    // Request failed, no need to store invalid handle.
    if (handle == rest::Handle{})
        return;

    NX_ASSERT(!m_handles.contains(handle));
    m_handles.insert(handle);
}

void ServerRequestStorage::releaseHandle(rest::Handle handle)
{
    // Note that the handle might not be there, and it's perfectly OK.
    m_handles.remove(handle);
}

void ServerRequestStorage::cancelAllRequests()
{
    if (auto api = connectedServerApi())
    {
        for (rest::Handle handle: m_handles)
            api->cancelRequest(handle);
    }
    m_handles.clear();
}

} // namespace nx::vms::client::desktop
