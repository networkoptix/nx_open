#pragma once

#include <QtCore/QSet>

#include <core/resource/resource_fwd.h>
#include <api/server_rest_connection_fwd.h>

namespace nx::vms::client::desktop {

/**
 * Utility class to be used together with `ServerConnection`.
 *
 * The main purpose is to disabuse the user from the need to keep track of all the handles returned
 * by the connection and make it possible to cancel all requests in destructor.
 *
 * See example usage in `WearableWorker` implementation.
 */
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

} // namespace nx::vms::client::desktop

