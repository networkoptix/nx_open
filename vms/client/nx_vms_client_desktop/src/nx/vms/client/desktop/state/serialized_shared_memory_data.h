// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "shared_memory_data.h"

namespace nx::vms::client::desktop {

using ScreensBitMask = quint64;

using SerializedSessionId = std::array<char, SessionId::kDataSize>;

using SerializedCloudUserName = std::array<char, SharedMemoryData::kCoudUserNameSize>;

struct SerializedSharedMemoryProcessData
{
    /** Serialized representation of the session id. */
    SerializedSessionId sessionId;

    SharedMemoryData::PidType pid = 0;

    /** Bitmask of the used screens. */
    ScreensBitMask usedScreens = 0;

    SharedMemoryData::Command command = SharedMemoryData::Command::none;

    /** Serialized representation of the command data. */
    std::array<char, SharedMemoryData::kCommandDataSize> commandData;

    SerializedCloudUserName cloudUserName = {};
};

struct SerializedSharedMemorySessionData
{
    SerializedSessionId id = {};
    std::array<char, SharedMemoryData::kTokenSize> token = {};
};

struct SerializedSharedMemoryCloudUserSession
{
    SerializedCloudUserName cloudUserName = {};
    std::array<char, SharedMemoryData::kTokenSize> refreshToken = {};
};

struct SerializedSharedMemoryData
{
    static constexpr int kDataVersion = 5;

    std::array<SerializedSharedMemoryProcessData, SharedMemoryData::kClientCount> processes;
    std::array<SerializedSharedMemorySessionData, SharedMemoryData::kClientCount> sessions;
    std::array<SerializedSharedMemoryCloudUserSession, SharedMemoryData::kClientCount> cloudUserSessions;

    static SharedMemoryData deserializedData(const SerializedSharedMemoryData& source);
    static void serializeData(const SharedMemoryData& source, SerializedSharedMemoryData* target);
};

} // namespace nx::vms::client::desktop
