#pragma once

#include <map>
#include <string>

#include <nx/network/http/http_async_client.h>
#include <nx/utils/move_only_func.h>

#include <nx/cloud/cdb/api/result_code.h>

namespace nx {
namespace cdb {

namespace conf { class Settings; }

enum class VmsResultCode
{
    ok,
    invalidData,
    networkError,
    forbidden,
    logicalError,
};

struct VmsRequestResult
{
    VmsResultCode resultCode = VmsResultCode::ok;
    std::string vmsErrorDescription;
};

using VmsRequestCompletionHandler = nx::utils::MoveOnlyFunc<void(VmsRequestResult)>;

class AbstractVmsGateway
{
public:
    virtual ~AbstractVmsGateway() = default;

    virtual void merge(
        const std::string& targetSystemId,
        VmsRequestCompletionHandler completionHandler) = 0;
};

class VmsGateway:
    public AbstractVmsGateway
{
public:
    VmsGateway(const conf::Settings& settings);
    virtual ~VmsGateway() override;

    virtual void merge(
        const std::string& targetSystemId,
        VmsRequestCompletionHandler completionHandler) override;

private:
    struct RequestContext
    {
        std::unique_ptr<nx_http::AsyncClient> client;
        VmsRequestCompletionHandler completionHandler;
    };

    const conf::Settings& m_settings;
    nx::network::aio::BasicPollable m_asyncCall;
    QnMutex m_mutex;
    std::map<nx_http::AsyncClient*, RequestContext> m_activeRequests;

    void reportRequestResult(nx_http::AsyncClient* clientPtr);
    VmsRequestResult prepareVmsResult(nx_http::AsyncClient* clientPtr);
};

} // namespace cdb
} // namespace nx
