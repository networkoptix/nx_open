#pragma once

#include <deque>
#include <memory>

#include <nx/utils/basic_factory.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/url.h>

#include "relay_api_client.h"

namespace nx::cloud::relay::api {

using ClientFactoryFunction =
    std::unique_ptr<Client>(const nx::utils::Url& baseUrl);

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
    void registerClientType(int priority = 0)
    {
        QnMutexLocker lock(&m_mutex);

        m_clientTypes.push_front(ClientTypeContext{
            priority,
            [](const nx::utils::Url& baseUrl, ClientFeedbackFunction feedbackFunction)
            {
                return std::make_unique<ClientType>(
                    baseUrl,
                    std::move(feedbackFunction));
            }});
    }

    static ClientFactory& instance();

private:
    using InternalFactoryFunction = nx::utils::MoveOnlyFunc<
        std::unique_ptr<Client>(const nx::utils::Url&, ClientFeedbackFunction)>;

    struct ClientTypeContext
    {
        int priority = 0;
        InternalFactoryFunction factoryFunction;
    };

    std::deque<ClientTypeContext> m_clientTypes;
    QnMutex m_mutex;

    std::unique_ptr<Client> defaultFactoryFunction(
        const nx::utils::Url& baseUrl);

    void processClientFeedback(
        std::deque<ClientTypeContext>::iterator clientTypeIter,
        ResultCode resultCode);
};

} // namespace nx::cloud::relay::api
