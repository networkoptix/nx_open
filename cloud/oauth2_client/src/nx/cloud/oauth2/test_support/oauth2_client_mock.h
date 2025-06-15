// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/network/http/http_types.h>

#include "../oauth2_client.h"

namespace nx::cloud::oauth2::client::test {

class NX_OAUTH2_CLIENT_API Oauth2ClientMockManager
{
public:
    using RequestPath = std::pair<std::string /* Path */, std::string_view /* Method */>;
    using RequestPathRegex = std::pair<std::regex /* Path */, std::string_view /* Method */>;
    using Response =
        std::pair<db::api::ResultCode /* result */, std::string /* serialized response*/>;

public:

    void setResponse(const RequestPath& requestPath, const Response& response);
    void setRequest(const RequestPath& requestPath, const std::string& request = "*");
    void setRequestPattern(const RequestPathRegex& requestPath, const Response& response);

    int getNumCalls(const RequestPath& requestPath);

private:
    friend class Oauth2ClientMock;

    struct PathHash
    {
        std::size_t operator()(const RequestPath& key) const
        {
            std::size_t h1 = std::hash<std::string>{}(key.first);
            std::size_t h2 = std::hash<std::string_view>{}(key.second);
            return h1 ^ (h2 << 1);
        }
    };

private:
    std::vector<std::pair<RequestPathRegex, Response>> m_requestPatterns;
    std::unordered_map<RequestPath, Response, PathHash> m_responses;
    std::unordered_map<RequestPath, std::string, PathHash> m_requests;
    std::unordered_map<RequestPath, int, PathHash> m_requestsCounter;
};

struct Oauth2MockResult
{
    db::api::ResultCode code = db::api::ResultCode::ok;
    std::string response;
};

class NX_OAUTH2_CLIENT_API Oauth2ClientMock: public AbstractOauth2Client
{
public:

    // In the dummy mode it doesn't perform any checks, just returns 200 OK.
    Oauth2ClientMock(Oauth2ClientMockManager& manager, bool dummyMode = false);

    void issueToken(
        const db::api::IssueTokenRequest& request,
        nx::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueTokenResponse)>
            completionHandler) override;

    void issueAuthorizationCode(
        const db::api::IssueTokenRequest& request,
        nx::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueCodeResponse)>
            completionHandler) override;

    void issuePasswordResetCode(
        const db::api::IssuePasswordResetCodeRequest& request,
        nx::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueCodeResponse)>
            completionHandler) override;

    void introspectToken(
        const db::api::TokenIntrospectionRequest& request,
        nx::MoveOnlyFunc<void(db::api::ResultCode, db::api::TokenIntrospectionResponse)>
            completionHandler) override;

    void logout(nx::MoveOnlyFunc<void(db::api::ResultCode)> completionHandler) override;

    virtual void clientLogout(
        const std::string& clientId,
        nx::MoveOnlyFunc<void(db::api::ResultCode)> completionHandler) override;

    void getJwtPublicKeys(
        nx::MoveOnlyFunc<void(db::api::ResultCode, std::vector<nx::network::jwk::Key>)>
            completionHandler) override;

    void getJwtPublicKeyByKid(
        const std::string& kid,
        nx::MoveOnlyFunc<void(db::api::ResultCode, nx::network::jwk::Key)>
            completionHandler) override;

    void introspectSession(
        const std::string& session,
        nx::MoveOnlyFunc<void(db::api::ResultCode, api::GetSessionResponse)>
            completionHandler) override;

    void markSessionMfaVerified(
        const std::string& sessionId,
        nx::MoveOnlyFunc<void(db::api::ResultCode)> handler) override;

    void notifyAccountUpdated(
        const db::api::AccountChangedEvent& /*event*/,
        nx::MoveOnlyFunc<void(db::api::ResultCode)> completionHandler) override;

protected:
    template <class Request, class Response, class CompletionHandler>
    void processRequest(
        const Oauth2ClientMockManager::RequestPath& requestPath,
        const Request& request,
        CompletionHandler handler);

    template<class Response, class CompletionHandler>
    void processRequest(
        const Oauth2ClientMockManager::RequestPath& requestPath,
        CompletionHandler handler,
        const Oauth2MockResult& result);

private:
    Oauth2ClientMockManager& m_manager;
    bool m_dummyMode = false;
};

template<class Request, class Response, class CompletionHandler>
void nx::cloud::oauth2::client::test::Oauth2ClientMock::processRequest(
    const Oauth2ClientMockManager::RequestPath& requestPath,
    const Request& request,
    CompletionHandler handler)
{
    std::string requestStr;
    if constexpr (std::is_same_v<Request, std::string>)
        requestStr = request;
    else
        requestStr = nx::reflect::json::serialize(request);

    std::optional<Oauth2MockResult> response;
    for (const auto& [pattern, reqResponse] : m_manager.m_requestPatterns)
    {
        if (pattern.second != requestPath.second)
            continue;
        if (std::regex_match(requestPath.first, pattern.first))
        {
            response = {reqResponse.first, reqResponse.second};
            break;
        }
    }
    if (!response)
    {
        if (!m_manager.m_requests.contains(requestPath))
        {
            NX_ASSERT(false, "Unexpected request path: %1", requestPath);
            return;
        }
        const std::string& registeredRequestStr = m_manager.m_requests[requestPath];
        if (registeredRequestStr != "*" && registeredRequestStr != requestStr)
        {
            NX_ASSERT(false, "Unexpected request for path %1: %2. Should be: %3",
                requestPath, requestStr, m_manager.m_requests[requestPath]);
        }
        response = Oauth2MockResult{
            m_manager.m_responses[requestPath].first,
            m_manager.m_responses[requestPath].second};
    }
    processRequest<Response>(requestPath, std::move(handler), *response);
}

template<class Response, class CompletionHandler>
void nx::cloud::oauth2::client::test::Oauth2ClientMock::processRequest(
    const Oauth2ClientMockManager::RequestPath& requestPath,
    CompletionHandler handler,
    const Oauth2MockResult& result)
{
    m_manager.m_requestsCounter[requestPath]++;
    if constexpr (!std::is_same_v<Response, void>)
    {
        Response response;
        nx::reflect::json::deserialize(result.response, &response);
        return handler(result.code, response);
    }
    else
    {
        return handler(result.code);
    }
}

} // namespace nx::cloud::oauth2::client::test
