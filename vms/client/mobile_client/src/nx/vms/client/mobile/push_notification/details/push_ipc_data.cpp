// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "push_ipc_data.h"

#include <QtCore/QtGlobal>

#if !defined(Q_OS_IOS) && !defined(Q_OS_ANDROID)

namespace nx::vms::client::mobile::details {

void PushIpcData::store(
    const std::string& /*user*/,
    const std::string& /*cloudRefreshToken*/,
    const std::string& /*password*/)
{
}

bool PushIpcData::load(
    std::string& user,
    std::string& cloudRefreshToken,
    std::string& password)
{
    user = std::string();
    cloudRefreshToken = std::string();
    password = std::string();
    return false;
}

void PushIpcData::clear()
{
}

PushIpcData::TokenInfo PushIpcData::accessToken(
    const std::string& /*cloudSystemId*/)
{
    return {};
}

void PushIpcData::setAccessToken(
    const std::string& /*cloudSystemId*/,
    const std::string& /*accessToken*/,
    const std::chrono::milliseconds& /*expiresAt*/)
{
}

void PushIpcData::removeAccessToken(const std::string& /*cloudSystemId*/)
{
}

bool PushIpcData::cloudLoggerOptions(
    std::string& logSessionId,
    std::chrono::milliseconds& sessionEndTime)
{
    return false;
}

void PushIpcData::setCloudLoggerOptions(
    const std::string& logSessionId,
    const std::chrono::milliseconds& sessionEndTime)
{
}

} // namespace nx::vms::client::mobile::details

#endif
