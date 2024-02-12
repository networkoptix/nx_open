// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_access_data.h"

#include <nx/utils/log/log.h>
#include <nx/vms/api/data/login.h>

namespace Qn {

UserAccessData::UserAccessData(nx::Uuid userId, Access access):
    userId(std::move(userId)),
    access(access)
{
}

UserAccessData::UserAccessData(
    nx::Uuid userId, Token token, Duration duration, TimePoint issued)
    :
    UserAccessData(std::move(userId))
{
    setToken(std::move(token), duration, issued);
}

UserAccessData::UserAccessData(
    nx::Uuid userId, Token token, Duration age, Duration expiresIn, TimePoint now)
    :
    UserAccessData(
        std::move(userId), std::move(token), /*duration*/ age + expiresIn, /*issued*/ now - age)
{
}

// TODO: In future versions all sessions should have normal timestamps, but for legacy auth we're
// going to use this really big value.
// TODO: Change to std::chrono::days when MSVC supports it.
constexpr const UserAccessData::Duration kDefaultDuration = std::chrono::hours(24) * 100;

UserAccessData::Duration UserAccessData::age(TimePoint now) const
{
    return m_issued ? std::chrono::ceil<Duration>(now - *m_issued) : kDefaultDuration;
}

UserAccessData::Duration UserAccessData::expiresIn(TimePoint now) const
{
    return (m_issued && m_duration.count() > 0) ? m_duration - age(now) : kDefaultDuration;
}

UserAccessData::TimePoint UserAccessData::issued() const
{
    return m_issued ? *m_issued : (nx::utils::monotonicTime() - kDefaultDuration);
}

void UserAccessData::setDuration(Duration duration)
{
    m_duration = std::move(duration);
}

void UserAccessData::setToken(Token token, Duration duration, std::optional<TimePoint> issued)
{
    m_token = std::move(token);
    m_issued = std::move(issued);
    m_duration = std::move(duration);
}

QString UserAccessData::toString() const
{
    const auto now = nx::utils::monotonicTime();
    static const std::string kNoToken("NO-TOKEN");
    return NX_FMT("%1(%2, %3, %4 old, for %5)",
        access, userId, m_token.empty() ? kNoToken : m_token, age(now), expiresIn(now));
}

QString toString(UserAccessData::Access access)
{
    switch (access)
    {
        case UserAccessData::Access::Default: return lit("Default");
        case UserAccessData::Access::ReadAllResources: return lit("ReadAllResources");
        case UserAccessData::Access::System: return lit("System");
    }

    const auto error = nx::format("Unknown=%1").arg(static_cast<int>(access));
    NX_ASSERT(false, error);
    return error;
}

const UserAccessData kSystemAccess(
    nx::Uuid("bc292159-2be9-4e84-a242-bc6122b315e4"),
    UserAccessData::Access::System);

const UserAccessData kVideowallUserAccess(
    nx::Uuid("1044d2a5-639d-4c49-963e-c03898d0c113"),
    UserAccessData::Access::ReadAllResources);

} //namespace Qn
