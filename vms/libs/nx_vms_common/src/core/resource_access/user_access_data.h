// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/time.h>
#include <nx/utils/uuid.h>
#include <core/resource/resource_fwd.h>

#include <nx/utils/literal.h>
#include <api/model/audit/auth_session.h>

namespace Qn {

/** Helper class to grant additional permissions (on the server side only). */
struct NX_VMS_COMMON_API UserAccessData
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

    QnUuid userId;
    Access access = Access::Default;

    explicit UserAccessData(
        QnUuid userId, Access access = Access::Default);
    explicit UserAccessData(
        QnUuid userId, Token token,
        Duration duration, TimePoint issued = nx::utils::monotonicTime());
    explicit UserAccessData(
        QnUuid userId, Token token,
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

    void setDuration(Duration duration);
    void setToken(
        Token token, Duration duration,
        std::optional<TimePoint> issued = nx::utils::monotonicTime());

    QString toString() const;

private:
    Token m_token;
    std::optional<TimePoint> m_issued;
    Duration m_duration;
};

struct UserSession
{
    UserSession(
        const UserAccessData& access = UserAccessData(),
        const QnAuthSession& session = QnAuthSession())
        :
        access(access),
        session(session)
    {
    }

    UserAccessData access;
    QnAuthSession session;
};

NX_VMS_COMMON_API QString toString(UserAccessData::Access access);

inline bool operator == (const UserAccessData &lhs, const UserAccessData &rhs)
{
    return lhs.userId == rhs.userId && lhs.access == rhs.access;
}

inline bool operator != (const UserAccessData &lhs, const UserAccessData &rhs)
{
    return ! operator == (lhs, rhs);
}

NX_VMS_COMMON_API extern const UserAccessData kSystemAccess;
NX_VMS_COMMON_API extern const UserAccessData kVideowallUserAccess;

}
