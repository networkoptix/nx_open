#pragma once

#include <map>
#include <memory>

#include <nx/utils/algorithm/item_selector.h>
#include <nx/utils/basic_factory.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/url.h>

#include "base_tunnel_client.h"

namespace nx_http::tunneling::detail {

using ClientFactoryFunction =
    std::unique_ptr<BaseTunnelClient>(
        const std::string& /*tag*/,
        const QUrl& /*baseUrl*/);

/**
 * Always instantiates client type with the highest priority.
 * Tunnel type priorioty is lowered with each tunnel failure.
 * Tunnel failure can be caused by some random short-term problems
 * (e.g., TCP connection broken or cloud service restart).
 * So, tunnel type can still be used. But, we do not know that.
 * So, we should not switch away the "best" tunnel type forever.
 * From time to time we should try to switch back.
 *
 * nx::utils::algorithm::ItemSelector is used to implement switching back.
 * So, with each problem, we sink tunnel type and we will try it when it pops up.
 */
class NX_NETWORK_API ClientFactory:
    public nx::utils::BasicFactory<ClientFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<ClientFactoryFunction>;

public:
    ClientFactory();

    template<typename ClientType>
    void registerClientType()
    {
        registerClientType(
            [](const QUrl& baseUrl, ClientFeedbackFunction feedbackFunction)
            {
                return std::make_unique<ClientType>(
                    baseUrl,
                    std::move(feedbackFunction));
            });
    }

    int topTunnelTypeId(const std::string& tag) const;

    void clear();

    static ClientFactory& instance();

private:
    using InternalFactoryFunction = nx::utils::MoveOnlyFunc<
        std::unique_ptr<BaseTunnelClient>(const QUrl&, ClientFeedbackFunction)>;

    using TunnelTypeSelector = nx::utils::algorithm::ItemSelector<int /*tunnel type id*/>;

    struct ClientTypeContext
    {
        InternalFactoryFunction factoryFunction;
    };

    std::map<int /*Tunnel type id*/, ClientTypeContext> m_clientTypes;
    mutable QnMutex m_mutex;
    int m_prevUsedTypeId = 0;
    std::map<std::string, TunnelTypeSelector> m_tagToTunnelTypeSelector;

    std::unique_ptr<BaseTunnelClient> defaultFactoryFunction(
        const std::string& tag,
        const QUrl& baseUrl);

    void registerClientType(InternalFactoryFunction factoryFunction);

    TunnelTypeSelector buildTunnelTypeSelector();

    void processClientFeedback(int typeId, const std::string& tag, bool success);
};

} // namespace nx_http::tunneling::detail
