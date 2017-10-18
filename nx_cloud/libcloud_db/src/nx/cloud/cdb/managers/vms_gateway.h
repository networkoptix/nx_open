#pragma once

#include <map>
#include <string>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/move_only_func.h>

#include <nx/cloud/cdb/api/result_code.h>

#include <api/mediaserver_client.h>

#include "account_manager.h"

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
    unreachable,
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(VmsResultCode)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((VmsResultCode), (lexical))

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
        const std::string& username,
        const std::string& targetSystemId,
        const std::string& systemIdToMergeTo,
        VmsRequestCompletionHandler completionHandler) = 0;
};

class VmsGateway:
    public AbstractVmsGateway
{
public:
    VmsGateway(
        const conf::Settings& settings,
        const AbstractAccountManager& accountManager);
    virtual ~VmsGateway() override;

    virtual void merge(
        const std::string& username,
        const std::string& targetSystemId,
        const std::string& systemIdToMergeTo,
        VmsRequestCompletionHandler completionHandler) override;

private:
    struct RequestContext
    {
        std::unique_ptr<MediaServerClient> client;
        std::string targetSystemId;
        VmsRequestCompletionHandler completionHandler;
    };

    const conf::Settings& m_settings;
    const AbstractAccountManager& m_accountManager;
    nx::network::aio::BasicPollable m_asyncCall;
    QnMutex m_mutex;
    std::map<MediaServerClient*, RequestContext> m_activeRequests;

    void reportInvalidConfiguration(
        const std::string& targetSystemId,
        VmsRequestCompletionHandler completionHandler);

    bool addAuthentication(
        const std::string& username,
        MediaServerClient* clientPtr);

    MergeSystemData prepareMergeRequestParameters(
        const std::string& systemIdToMergeTo);

    void reportRequestResult(
        MediaServerClient* clientPtr,
        QnJsonRestResult result);

    VmsRequestResult prepareVmsResult(
        MediaServerClient* clientPtr,
        const QnJsonRestResult& result);

    VmsRequestResult convertVmsResultToResultCode(
        MediaServerClient* clientPtr,
        const QnJsonRestResult& vmsResult);
};

} // namespace cdb
} // namespace nx
