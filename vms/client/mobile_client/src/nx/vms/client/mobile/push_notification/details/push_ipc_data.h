// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <string>

namespace nx::vms::client::mobile::details {

/**
 * Helper class for storing interprocess data. Uses secured storage which is available
 * both for the mobile application and notification service process/extention. In Android it is
 * encrypted shared properties, in iOS it is keychain storage.
 */
struct PushIpcData
{
    struct TokenInfo
    {
        std::string accessToken;
        std::chrono::milliseconds expiresAt{};
    };

    /**
     *  Stores data to the secured storage.
     *  @user Currently logged cloud user.
     *  @refreshToken Refresh token of the current connection to the cloud.
     *  @password Password for the compatibility with servers with version < 5.0.
     */
    static void store(
        const std::string& user,
        const std::string& cloudRefreshToken,
        const std::string& password);

    /** Retrives data from the secured storage. */
    static bool load(
        std::string& user,
        std::string& cloudRefreshToken,
        std::string& password);

    /** Clears data in the secured storage. */
    static void clear();

    /**
     *  Return stored access token for the specified cloud system to retrieve media data for
     *  the push notification.
     */
    static TokenInfo accessToken(const std::string& cloudSystemId);

    /**
     *  Set access token for the specified cloud system that could be used to retrieve media
     *  data for the push notification.
     */
    static void setAccessToken(
        const std::string& cloudSystemId,
        const std::string& accessToken,
        const std::chrono::milliseconds& expiresAt);

    /** Remove access token for the specified cloud system. */
    static void removeAccessToken(const std::string& cloudSystemId);

    /** Get parameters of the remote cloud logging session. */
    static bool cloudLoggerOptions(
        std::string& logSessionId,
        std::chrono::milliseconds& sessionEndTime);

    /** Set parameters of the remote cloud logging session. */
    static void setCloudLoggerOptions(
        const std::string& logSessionId,
        const std::chrono::milliseconds& sessionEndTime);

private:
    PushIpcData() = delete;
};

} // namespace nx::vms::client::mobile::details
