#pragma once

#include <QtCore/QUrl>

#include <nx/network/http/http_async_client.h>

#include "listening_peer.h"

namespace nx {
namespace hpm {
namespace api {

// TODO: #ak Refactor this class and move to an appropriate place.
class NX_NETWORK_API Client
{
public:
    Client(const QUrl& baseMediatorApiUrl);

    std::tuple<nx_http::StatusCode::Value, ListeningPeers> getListeningPeers() const;

private:
    const QUrl m_baseMediatorApiUrl;
};

} // namespace api
} // namespace hpm
} // namespace nx
