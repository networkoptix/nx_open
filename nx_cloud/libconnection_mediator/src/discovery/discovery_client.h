#pragma once

#include <string>
#include <type_traits>
#include <vector>

#include <QtCore/QUrl>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/aio/basic_pollable.h>

namespace nx {
namespace cloud {
namespace discovery {

enum class ResultCode
{
    ok,
    notFound,
    networkError,
    invalidModuleInformation,
};

enum class PeerStatus
{
    online,
    offline,
};

} // namespace nx
} // namespace cloud
} // namespace discovery

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (nx::cloud::discovery::ResultCode)(nx::cloud::discovery::PeerStatus),
    (lexical))

namespace nx {
namespace cloud {
namespace discovery {

struct BasicInstanceInformation
{
    std::string id;
    QUrl apiUrl;
    PeerStatus status = PeerStatus::offline;
};

#define BasicInstanceInformation_Fields (id)(apiUrl)(status)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (BasicInstanceInformation),
    (json))

//-------------------------------------------------------------------------------------------------

/**
 * @param InstanceInformation Must inherit BasicInstanceInformation 
 * and define static member kTypeName (e.g. relay, mediator, etc...).
 */
template<typename InstanceInformation>
class ModuleRegistrar:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

    static_assert(
        std::is_base_of<BasicInstanceInformation, InstanceInformation>::value,
        "InstanceInformation MUST be a descendant of BasicInstanceInformation");

public:
    ModuleRegistrar(const QUrl& /*baseUrl*/)
    {
        // TODO
    }

    void setInstanceInformation(InstanceInformation /*instanceInformation*/)
    {
        // TODO
    }

    void start()
    {
        // TODO
    }
};

//-------------------------------------------------------------------------------------------------

class ModuleFinder
{
public:
    ModuleFinder(const QUrl& /*baseUrl*/)
    {
        // TODO
    }

    template<typename InstanceInformation>
    void fetchModules(
        nx::utils::MoveOnlyFunc<void(ResultCode, std::vector<InstanceInformation>)> /*handler*/)
    {
        // TODO
    }
};

} // namespace nx
} // namespace cloud
} // namespace discovery
