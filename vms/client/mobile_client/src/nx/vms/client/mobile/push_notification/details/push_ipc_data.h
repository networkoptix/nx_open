// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <string>

namespace nx::vms::client::mobile::details {

/**
 * Helper class for storing interprocess data. Uses secured storage which is available
 * both for the mobile application and notification service process/extension. In Android it is
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
     *  @user Currently logged cloud user. Converts it to the lowercase while storing.
     *  @refreshToken Refresh token of the current connection to the cloud.
     */
    static void store(const std::string& user, const std::string& cloudRefreshToken);

    /** Retrieves data from the secured storage. */
    static bool load(std::string& user, std::string& cloudRefreshToken);

    /** Clears data in the secured storage. */
    static void clear();

    /**
     *  Return stored access token for the specified cloud system to retrieve media data for
     *  the push notification. Returns an empty token if the stored one has expired or was issued
     *  for another user.
     */
    static TokenInfo accessToken(const std::string& cloudSystemId, const std::string& user);

    /**
     *  Set access token issued for the specified user and cloud system that could be used to
     *  retrieve media data for the push notification.
     */
    static void setAccessToken(const std::string& cloudSystemId,
        const std::string& user,
        const std::string& accessToken,
        const std::chrono::milliseconds& expiresAt);

    /**
     *  Remove access token for the specified cloud system if it is still the stored one. Another
     *  token saved after this one was loaded is kept.
     */
    static void removeAccessToken(
        const std::string& cloudSystemId, const std::string& accessToken);

    /** Remove access tokens of all cloud systems. */
    static void clearAccessTokens();

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

    static void storeImpl(const std::string& user, const std::string& cloudRefreshToken);
};

} // namespace nx::vms::client::mobile::details
