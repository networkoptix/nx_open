#pragma once

#include <QtCore/QUrl>

#include <nx/network/http/http_async_client.h>
#include <nx/utils/url.h>

#include "listening_peer.h"

namespace nx {
namespace hpm {
namespace api {

// TODO: #ak Refactor this class and move to an appropriate place.
class NX_NETWORK_API Client
{
public:
    Client(const nx::utils::Url& baseMediatorApiUrl);

    std::tuple<nx::network::http::StatusCode::Value, ListeningPeers> getListeningPeers() const;

private:
    const nx::utils::Url m_baseMediatorApiUrl;
};

} // namespace api
} // namespace hpm
} // namespace nx
