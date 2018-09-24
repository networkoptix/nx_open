#pragma once

#include <deque>
#include <memory>

#include <nx/utils/basic_factory.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/url.h>

#include "base_tunnel_client.h"

namespace nx_http::tunneling::detail {

using ClientFactoryFunction =
    std::unique_ptr<BaseTunnelClient>(const QUrl& baseUrl);

/**
 * Always instantiates client type with the highest priority.
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
        QnMutexLocker lock(&m_mutex);

        m_clientTypes.push_front(ClientTypeContext{
            ++m_prevUsedTypeId,
            [](const QUrl& baseUrl, ClientFeedbackFunction feedbackFunction)
            {
                return std::make_unique<ClientType>(
                    baseUrl,
                    std::move(feedbackFunction));
            }});
    }

    void clear();

    static ClientFactory& instance();

private:
    using InternalFactoryFunction = nx::utils::MoveOnlyFunc<
        std::unique_ptr<BaseTunnelClient>(const QUrl&, ClientFeedbackFunction)>;

    struct ClientTypeContext
    {
        int id = 0;
        InternalFactoryFunction factoryFunction;
    };

    std::deque<ClientTypeContext> m_clientTypes;
    QnMutex m_mutex;
    int m_prevUsedTypeId = 0;

    std::unique_ptr<BaseTunnelClient> defaultFactoryFunction(
        const QUrl& baseUrl);

    void processClientFeedback(int typeId, bool success);
};

} // namespace nx_http::tunneling::detail
