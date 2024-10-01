// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "serialized_shared_memory_data.h"

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

namespace {

static_assert(SessionId::kDataSize == 32, "Data version must be increased if changed.");
static_assert(SharedMemoryData::kClientCount == 64, "Data version must be increased if changed.");
static_assert(
    sizeof(SharedMemoryData::Command) == 4, "Data version must be increased if changed.");
static_assert(
    SharedMemoryData::kCommandDataSize == 64, "Data version must be increased if changed.");
static_assert(
    SharedMemoryData::kTokenSize == 8192, "Data version must be increased if changed.");
static_assert(
    SharedMemoryData::kCoudUserNameSize == 255, "Data version must be increased if changed.");


ScreensBitMask serializedScreens(const SharedMemoryData::ScreenUsageData& screens)
{
    ScreensBitMask result = 0;
    for (int screen: screens)
        result |= 1ull << screen;
    return result;
}

SharedMemoryData::ScreenUsageData deserializedScreens(ScreensBitMask data)
{
    SharedMemoryData::ScreenUsageData screens;
    for (int index = 0; data; ++index)
    {
        if (data & 1)
            screens.insert(index);
        data = data >> 1;
    }
    return screens;
}

SerializedSharedMemoryProcessData serializedProcessData(const SharedMemoryData::Process& source)
{
    QByteArray sessionId = source.sessionId.serialized();

    SerializedSharedMemoryProcessData result;
    std::copy(sessionId.cbegin(), sessionId.cend(), result.sessionId.begin());
    result.pid = source.pid;
    result.usedScreens = serializedScreens(source.usedScreens);
    result.command = source.command;
    result.commandData.fill(0);
    QByteArray commandData = source.commandData;
    commandData.truncate(SharedMemoryData::kCommandDataSize);
    std::copy(commandData.cbegin(), commandData.cend(), result.commandData.begin());
    std::copy(
        source.cloudUserName.begin(), source.cloudUserName.end(), result.cloudUserName.begin());
    return result;
}

SerializedSharedMemorySessionData serializedSessionData(const SharedMemoryData::Session& session)
{
    SerializedSharedMemorySessionData result;
    const QByteArray sessionId = session.id.serialized();
    std::copy(sessionId.begin(), sessionId.end(), result.id.begin());

    NX_ASSERT(session.token.size() + /*null terminator*/ 1 <= SharedMemoryData::kTokenSize,
        "Invalid token size");

    std::copy(session.token.begin(), session.token.end(), result.token.begin());
    return result;
}

SerializedSharedMemoryCloudUserSession serializedCloudUserSessions(
    const SharedMemoryData::CloudUserSession& cloudUserSession)
{
    SerializedSharedMemoryCloudUserSession result;
    std::copy(cloudUserSession.cloudUserName.begin(),
        cloudUserSession.cloudUserName.end(),
        result.cloudUserName.begin());

    NX_ASSERT(
        cloudUserSession.refreshToken.size() + /*null terminator*/ 1 <= SharedMemoryData::kTokenSize,
        "Invalid token size");

    std::copy(cloudUserSession.refreshToken.begin(),
        cloudUserSession.refreshToken.end(),
        result.refreshToken.begin());
    return result;
}

SharedMemoryData::Process deserializedProcessData(const SerializedSharedMemoryProcessData& source)
{
    SharedMemoryData::Process result;
    result.sessionId = SessionId::deserialized({source.sessionId.data(), SessionId::kDataSize});
    result.pid = source.pid;
    result.usedScreens = deserializedScreens(source.usedScreens);
    result.command = source.command;

    int commandDataSize = SharedMemoryData::kCommandDataSize;
    if (std::find(source.commandData.cbegin(), source.commandData.cend(), 0)
        != source.commandData.cend())
    {
        commandDataSize = -1; // Auto-detect QByteArray length as a null-terminated string.
    }
    result.commandData = QByteArray(source.commandData.data(), commandDataSize);
    result.cloudUserName = source.cloudUserName.data();
    return result;
}

SharedMemoryData::Session deserializedSessionData(const SerializedSharedMemorySessionData& source)
{
    SharedMemoryData::Session result;
    result.id = SessionId::deserialized({source.id.data(), SessionId::kDataSize});
    NX_ASSERT(source.token.back() == '\0');
    result.token = source.token.data();
    return result;
}

SharedMemoryData::CloudUserSession deserializedCloudUserSessions(
    const SerializedSharedMemoryCloudUserSession& source)
{
    SharedMemoryData::CloudUserSession result;
    result.cloudUserName = source.cloudUserName.data();
    NX_ASSERT(source.refreshToken.back() == '\0');
    result.refreshToken = source.refreshToken.data();
    return result;
}

} // namespace

SharedMemoryData SerializedSharedMemoryData::deserializedData(
    const SerializedSharedMemoryData& source)
{
    SharedMemoryData result;
    std::transform(source.processes.cbegin(),
        source.processes.cend(),
        result.processes.begin(),
        deserializedProcessData);
    std::transform(source.sessions.cbegin(),
        source.sessions.cend(),
        result.sessions.begin(),
        deserializedSessionData);
    std::transform(source.cloudUserSessions.cbegin(),
        source.cloudUserSessions.cend(),
        result.cloudUserSessions.begin(),
        deserializedCloudUserSessions);
    return result;
}

void SerializedSharedMemoryData::serializeData(
    const SharedMemoryData& source, SerializedSharedMemoryData* target)
{
    std::transform(source.processes.cbegin(),
        source.processes.cend(),
        target->processes.begin(),
        serializedProcessData);
    std::transform(source.sessions.cbegin(),
        source.sessions.cend(),
        target->sessions.begin(),
        serializedSessionData);
    std::transform(source.cloudUserSessions.cbegin(),
        source.cloudUserSessions.cend(),
        target->cloudUserSessions.begin(),
        serializedCloudUserSessions);
}

} // namespace nx::vms::client::desktop
