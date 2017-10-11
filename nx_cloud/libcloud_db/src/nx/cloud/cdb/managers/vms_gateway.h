#pragma once

#include <map>

#include <nx/network/http/http_async_client.h>
#include <nx/utils/move_only_func.h>

namespace nx {
namespace cdb {

namespace conf { class Settings; }

class AbstractVmsGateway
{
public:
    virtual ~AbstractVmsGateway() = default;

    virtual void merge(
        const std::string& targetSystemId,
        nx::utils::MoveOnlyFunc<void()> completionHandler) = 0;
};

class VmsGateway:
    public AbstractVmsGateway
{
public:
    VmsGateway(const conf::Settings& settings);
    virtual ~VmsGateway() override;

    virtual void merge(
        const std::string& targetSystemId,
        nx::utils::MoveOnlyFunc<void()> completionHandler) override;

private:
    struct RequestContext
    {
        std::unique_ptr<nx_http::AsyncClient> client;
        nx::utils::MoveOnlyFunc<void()> completionHandler;
    };

    const conf::Settings& m_settings;
    nx::network::aio::BasicPollable m_asyncCall;
    QnMutex m_mutex;
    std::map<nx_http::AsyncClient*, RequestContext> m_activeRequests;

    void reportRequestResult(nx_http::AsyncClient* clientPtr);
};

} // namespace cdb
} // namespace nx
