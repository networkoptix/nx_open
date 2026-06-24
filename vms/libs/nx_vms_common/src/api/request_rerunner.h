// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/network/http/auth_tools.h>
#include <nx/utils/coro/task.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/utils/abstract_session_token_helper.h>

namespace nx::vms::common {

// A helper class to rerun requests that require a fresh authorization token.
// For security reasons, some privileged API requests can be executed only with a recently
// issued authorization token. If the token is not fresh enough, such request will fail,
// returning an error, and the user should be asked to enter his password and authorize again.
// After that the request is resend with a new authorization token. Since the Client can send
// several privileged request simultaneously, they all could fail at once. Only one
// authorization dialog should be shown in that case. Also it's possible that responses for
// some failed requests will be delivered after this dialog has been closed (e.g. because of a
// slow internet connection), and another dialog should not be shown in that case.
// The user may also dismiss the authorization dialog instead of entering the password. Whether
// the token refresh failed because of such cancellation or because of an actual error is
// reported via RefreshError (userCancelled vs internalError).
class RequestRerunner
{
public:
    using RefreshError = AbstractSessionTokenHelper::RefreshError;
    using RefreshResult = AbstractSessionTokenHelper::RefreshResult;

    nx::coro::Task<RefreshResult> getNewToken(
        nx::vms::common::SessionTokenHelperPtr helper);

private:
    using ResendRequestFunc = nx::MoveOnlyFunc<void(RefreshResult)>;

    auto receiveToken()
    {
        struct TokenAwaiter
        {
            TokenAwaiter(RequestRerunner* rerunner): m_rerunner(rerunner)
            {
            }

            bool await_ready() const { return false; }

            void await_suspend(std::coroutine_handle<> h)
            {
                auto guard = nx::utils::makeScopeGuard([h]() mutable { h(); });
                m_rerunner->m_resumableRequests.push_back(
                    [h = std::move(h), guard = std::move(guard), this](
                        RefreshResult result) mutable
                    {
                        guard.disarm();
                        m_result = std::move(result);
                        h();
                    });
            }

            RefreshResult await_resume() noexcept
            {
                return std::move(m_result);
            }

            RequestRerunner* const m_rerunner;
            RefreshResult m_result{std::unexpected(RefreshError::internalError)};
        };

        return TokenAwaiter{this};
    }

    bool hasRequestsToRerun() const { return !m_resumableRequests.empty(); }

    void rerunRequests(const RefreshResult& result)
    {
        decltype(m_resumableRequests) awaitingRequests;
        std::swap(awaitingRequests, m_resumableRequests);

        for (auto& request: awaitingRequests)
        {
            if (auto callback = std::move(request))
                callback(result);
        }
    }

private:
    std::vector<ResendRequestFunc> m_resumableRequests;
};

} // namespace nx::vms::common
