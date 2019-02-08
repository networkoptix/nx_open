#pragma once

#include <map>
#include <string>

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/move_only_func.h>

#include <nx/cloud/db/api/result_code.h>

#include <api/mediaserver_client.h>

#include "account_manager.h"
#include "vms_request_result.h"

namespace nx::cloud::db {

namespace conf { class Settings; }

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

    MergeSystemData prepareMergeRequestParameters(
        const nx::network::http::Credentials& userCredentials,
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

} // namespace nx::cloud::db
