// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <memory>

#include <nx/utils/algorithm/item_selector.h>
#include <nx/utils/basic_factory.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/url.h>

#include "base_tunnel_client.h"

namespace nx::network::http::tunneling::detail {

using ClientFactoryFunction =
    std::vector<std::unique_ptr<BaseTunnelClient>>(
        const std::string& /*tag*/,
        const nx::utils::Url& /*baseUrl*/,
        std::optional<int> /*forcedTunnelType*/,
        const ConnectOptions& /*options*/);

/**
 * Always instantiates client type with the highest priority.
 * Tunnel type priority is lowered with each tunnel failure.
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

    /**
     * @return Generated numeric id for the registered type.
     */
    template<typename ClientType>
    int registerClientType(int initialPriority = 0)
    {
        return registerClientType(
            [](
                const nx::utils::Url& baseUrl,
                const ConnectOptions& options,
                ClientFeedbackFunction feedbackFunction)
            {
                return std::make_unique<ClientType>(
                    baseUrl,
                    options,
                    std::move(feedbackFunction));
            },
            initialPriority);
    }

    /**
     * @return Tunnel types that are to be chosen to establish connection.
     */
    std::vector<int> topTunnelTypeIds(const std::string& tag) const;

    void clear();

    /**
     * Resets the object to the initial state.
     */
    void reset();

    template<typename ClientType>
    void setForcedClientType()
    {
        setForcedClientFactory(
            [](
                const nx::utils::Url& baseUrl,
                const ConnectOptions& options,
                ClientFeedbackFunction feedbackFunction)
            {
                return std::make_unique<ClientType>(
                    baseUrl,
                    options,
                    std::move(feedbackFunction));
            });
    }

    void removeForcedClientType();

    static ClientFactory& instance();

private:
    using InternalFactoryFunction = nx::utils::MoveOnlyFunc<
        std::unique_ptr<BaseTunnelClient>(
            const nx::utils::Url&,
            const ConnectOptions&,
            ClientFeedbackFunction)>;

    using TunnelTypeSelector = nx::utils::algorithm::ItemSelector<int /*tunnel type id*/>;

    struct ClientTypeContext
    {
        InternalFactoryFunction factoryFunction;
        int initialPriority = 0;
    };

    std::map<int /*Tunnel type id*/, ClientTypeContext> m_clientTypes;
    mutable nx::Mutex m_mutex;
    int m_prevUsedTypeId = 0;
    mutable std::map<std::string, TunnelTypeSelector> m_tagToTunnelTypeSelector;
    InternalFactoryFunction m_forcedClientFactory;

    std::vector<std::unique_ptr<BaseTunnelClient>> defaultFactoryFunction(
        const std::string& tag,
        const nx::utils::Url& baseUrl,
        std::optional<int> forcedTunnelType,
        const ConnectOptions& options);

    int registerClientType(
        InternalFactoryFunction factoryFunction,
        int initialPriority);

    TunnelTypeSelector buildTunnelTypeSelector() const;

    void processClientFeedback(int typeId, const std::string& tag, bool success);

    void setForcedClientFactory(InternalFactoryFunction func);
};

} // namespace nx::network::http::tunneling::detail
