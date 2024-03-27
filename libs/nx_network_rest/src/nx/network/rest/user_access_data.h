// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>

#include <QtCore/QString>

#include <nx/utils/time.h>
#include <nx/utils/uuid.h>

#include "auth_session.h"

namespace nx::network::rest {

/** Helper class to grant additional permissions (on the server side only). */
struct NX_NETWORK_REST_API UserAccessData
{
    enum class Access
    {
        Default,            /**< Default access rights. All permissions are checked as usual. */
        ReadAllResources,   /**< Read permission on all resources except of users is granted additionally. */
        System              /**< Full permissions on all transactions. */
    };

    using Token = std::string;
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = std::chrono::milliseconds;

    nx::Uuid userId;
    Access access = Access::Default;

    explicit UserAccessData(
        nx::Uuid userId, Access access = Access::Default);
    explicit UserAccessData(
        nx::Uuid userId, Token token,
        Duration duration, TimePoint issued = nx::utils::monotonicTime());
    explicit UserAccessData(
        nx::Uuid userId, Token token,
        Duration age, Duration expiresIn, TimePoint now = nx::utils::monotonicTime());

    UserAccessData() = default;
    UserAccessData(const UserAccessData&) = default;
    UserAccessData(UserAccessData&&) = default;

    UserAccessData& operator=(const UserAccessData&) = default;
    UserAccessData& operator=(UserAccessData&&) = default;

    bool isNull() const { return userId.isNull(); }

    const Token& token() const { return m_token; }
    Duration age(TimePoint now = nx::utils::monotonicTime()) const;
    Duration expiresIn(TimePoint now = nx::utils::monotonicTime()) const;
    TimePoint issued() const;

    std::optional<Duration> userDuration() const;
    void setUserDuration(std::optional<Duration> duration);

    void setSiteDuration(Duration duration);
    Duration siteDuration() const;

    void setToken(
        Token token, Duration duration,
        std::optional<TimePoint> issued = nx::utils::monotonicTime());

    QString toString() const;

private:
    Token m_token;
    std::optional<TimePoint> m_issued;
    Duration m_siteDuration{0};
    std::optional<Duration> m_userDuration;
};

struct UserSession
{
    UserAccessData access;
    AuthSession session;
};

NX_NETWORK_REST_API QString toString(UserAccessData::Access access);

inline bool operator==(const UserAccessData& lhs, const UserAccessData& rhs)
{
    return lhs.userId == rhs.userId && lhs.access == rhs.access;
}

inline bool operator!=(const UserAccessData& lhs, const UserAccessData& rhs)
{
    return !operator==(lhs, rhs);
}

NX_NETWORK_REST_API extern const UserSession kSystemSession;
NX_NETWORK_REST_API extern const UserAccessData kSystemAccess;
NX_NETWORK_REST_API extern const UserAccessData kVideowallUserAccess;

} // namespace nx::network::rest
