// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSet>

#include <api/server_rest_connection_fwd.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>

namespace nx::vms::client::desktop {

/**
 * Utility class to be used together with `ServerConnection`.
 *
 * The main purpose is to disabuse the user from the need to keep track of all the handles returned
 * by the connection and make it possible to cancel all requests in destructor.
 *
 * See example usage in `VirtualCameraWorker` implementation.
 */
class ServerRequestStorage: public core::RemoteConnectionAware
{
public:
    ServerRequestStorage();
    ~ServerRequestStorage();

    void storeHandle(rest::Handle handle);
    void releaseHandle(rest::Handle handle);
    void cancelAllRequests();

private:
    QSet<rest::Handle> m_handles;
};

} // namespace nx::vms::client::desktop
