// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/http_types.h>

#include "../oauth2_client.h"

namespace nx::cloud::oauth2::client::test {

class Oauth2ClientMockManager
{
public:
    using RequstPath = std::pair<std::string /* Path */, std::string_view /* Method */>;
    using Response =
        std::pair<db::api::ResultCode /* result */, std::string /* serialized response*/>;

public:
    void setResponse(const RequstPath& requestPath, const Response& response);
    void setRequest(const RequstPath& requestPath, const std::string& request);
    int getNumCalls(const RequstPath& requestPath);

private:
    friend class Oauth2ClientMock;

    struct PathHash
    {
        std::size_t operator()(const RequstPath& key) const
        {
            std::size_t h1 = std::hash<std::string>{}(key.first);
            std::size_t h2 = std::hash<std::string_view>{}(key.second);
            return h1 ^ (h2 << 1);
        }
    };

private:
    std::unordered_map<RequstPath, Response, PathHash> m_responses;
    std::unordered_map<RequstPath, std::string, PathHash> m_requests;
    std::unordered_map<RequstPath, int, PathHash> m_requestsCounter;
};

class Oauth2ClientMock : public AbstractOauth2Client
{
public:
    Oauth2ClientMock(Oauth2ClientMockManager& manager);

    void issueToken(
        const db::api::IssueTokenRequest& request,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueTokenResponse)>
            completionHandler) override;

    void issueAuthorizationCode(
        const db::api::IssueTokenRequest& request,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueCodeResponse)>
            completionHandler) override;

    void introspectToken(
        const db::api::TokenIntrospectionRequest& request,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode, db::api::TokenIntrospectionResponse)>
            completionHandler) override;

    void logout(nx::utils::MoveOnlyFunc<void(db::api::ResultCode)> completionHandler) override;

    void getJwtPublicKeys(
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode, std::vector<nx::network::jwk::Key>)>
            completionHandler) override;

    void getJwtPublicKeyByKid(
        const std::string& kid,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode, nx::network::jwk::Key)>
            completionHandler) override;

    void introspectSession(
        const std::string& session,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode, api::GetSessionResponse)>
            completionHandler) override;

    void internalClientLogout(
        const std::string& email,
        const std::string& clienId,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode)> handler) override;

    void internalLogout(
        const std::string& email, nx::utils::MoveOnlyFunc<void(db::api::ResultCode)> handler) override;

    void internalIssueToken(
        const std::optional<std::string>& hostName,
        const network::SocketAddress& clientEndpoint,
        const nx::cloud::db::api::IssueTokenRequest& request,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode, db::api::IssueTokenResponse)> handler) override;

    void internalGetSession(
        const std::string& sessionId,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode, nx::cloud::db::api::AuthSession)>
            handler) override;

    void markSessionMfaVerified(
        const std::string& sessionId,
        nx::utils::MoveOnlyFunc<void(db::api::ResultCode)> handler) override;

private:
    template <class Request, class Response, class CompletionHandler>
    void processRequst(
        const Oauth2ClientMockManager::RequstPath& requestPath,
        const Request& request,
        CompletionHandler handler);

    template<class Response, class CompletionHandler>
    void processRequst(
        const Oauth2ClientMockManager::RequstPath& requestPath,
        CompletionHandler handler);

private:
    Oauth2ClientMockManager& m_manager;
};

template<class Request, class Response, class CompletionHandler>
void nx::cloud::oauth2::client::test::Oauth2ClientMock::processRequst(
    const Oauth2ClientMockManager::RequstPath& requestPath,
    const Request& request,
    CompletionHandler handler)
{
    std::string requestStr;
    if constexpr (std::is_same_v<Request, std::string>)
        requestStr = request;
    else
        requestStr = nx::reflect::json::serialize(request);

    NX_ASSERT(m_manager.m_requests.count(requestPath));
    NX_ASSERT(m_manager.m_requests[requestPath] == requestStr);
    processRequst<Response>(requestPath, std::move(handler));
}

template<class Response, class CompletionHandler>
void nx::cloud::oauth2::client::test::Oauth2ClientMock::processRequst(
    const Oauth2ClientMockManager::RequstPath& requestPath, CompletionHandler handler)
{
    m_manager.m_requestsCounter[requestPath]++;
    if constexpr (!std::is_same_v<Response, void>)
    {
        Response response;
        nx::reflect::json::deserialize(m_manager.m_responses[requestPath].second, &response);
        return handler(m_manager.m_responses[requestPath].first, response);
    }
    else
    {
        return handler(m_manager.m_responses[requestPath].first);
    }
}

} // namespace nx::cloud::oauth2::client::test
