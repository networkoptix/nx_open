#include "discovery_client.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace cloud {
namespace discovery {

bool operator==(const BasicInstanceInformation& left, const BasicInstanceInformation& right)
{
    return left.type == right.type
        && left.id == right.id
        && left.apiUrl == right.apiUrl;
}

//-------------------------------------------------------------------------------------------------

ModuleFinder::ModuleFinder(const utils::Url &baseUrl):
    m_baseUrl(baseUrl)
{
}

void ModuleFinder::bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    for (auto& request : m_runningRequests)
        request->bindToAioThread(aioThread);
}

void ModuleFinder::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();
    m_runningRequests.clear();
}

//-------------------------------------------------------------------------------------------------

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (BasicInstanceInformation),
    (json),
    _Fields)

} // namespace discovery
} // namespace cloud
} // namespace nx

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cloud::discovery, ResultCode,
    (nx::cloud::discovery::ResultCode::ok, "ok")
    (nx::cloud::discovery::ResultCode::notFound, "notFound")
    (nx::cloud::discovery::ResultCode::networkError, "networkError")
    (nx::cloud::discovery::ResultCode::invalidModuleInformation, "invalidModuleInformation")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cloud::discovery, PeerStatus,
    (nx::cloud::discovery::PeerStatus::online, "online")
    (nx::cloud::discovery::PeerStatus::offline, "offline")
)
