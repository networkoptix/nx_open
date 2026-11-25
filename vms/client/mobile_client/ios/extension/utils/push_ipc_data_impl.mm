// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <functional>
#include <mutex>
#include <sstream>
#include <vector>

#include <Foundation/Foundation.h>
#include <Security/Security.h>

#include <nx/vms/client/mobile/push_notification/details/push_ipc_data.h>

#include "ipc_access_group.h"
#include "logger.h"

namespace {

static const auto kCloudLoggerOptionTag = @"cloud_logger_options";
static const auto kPushCredentialsTag = @"last_push_credentials";
static const auto kAccessTokensTag = @"access_tokens";
static const auto kDelimiter = '\n';
static const auto kAccessGroup =
    [NSString stringWithUTF8String: nx::vms::client::mobile::details::kIpcAccessGroup];

// Access tokens are changed by loading, changing and saving all of them, so the reads and changes
// made in parallel inside one process must not overlap.
std::mutex accessTokensMutex;

using namespace extension;

using LogFunction = std::function<void (const NSString* logTag, const NSString* message)>;
LogFunction logFunction(bool uploadLogsToCloud)
{
    return
        [uploadLogsToCloud](const NSString* logTag, const NSString* message)
        {
            if (uploadLogsToCloud)
                Logger::log(logTag, message);
            else
                Logger::logToSystemOnly(logTag, message);
        };
}

NSData* loadDataForTag(
    const NSString* valueTag,
    const NSString* logTag,
    bool uploadLogsToCloud = true)
{
    const auto log = logFunction(uploadLogsToCloud);

    const auto query = @{
        (id) kSecClass: (id) kSecClassGenericPassword,
        (id) kSecAttrAccount: (id) valueTag,
        (id) kSecMatchLimit: (id) kSecMatchLimitOne,
        (id) kSecReturnData: (id) kCFBooleanTrue,
        (id) kSecAttrAccessGroup: kAccessGroup,
        (id) kSecAttrAccessible: (id) kSecAttrAccessibleAfterFirstUnlock};

    CFTypeRef result;
    const auto status = SecItemCopyMatching((__bridge CFDictionaryRef) query, &result);
    if (status != errSecSuccess)
    {
        log(logTag, [NSString stringWithFormat: @"can't load data, status is %d", status]);
        return nil;
    }

    // SecItemCopyMatching returns a retained object, and this file is built without ARC.
    const auto data = [(__bridge NSData*) result autorelease];

    if (!data)
    {
        log(logTag, @"keychain error, the data is empty.");
        return nil;
    }

    log(logTag, @"data was loaded.");
    return data;
}

void saveDataForTag(
    const NSString* valueTag,
    const NSData* data,
    const NSString* logTag,
    bool uploadLogsToCloud = true)
{
    const auto log = logFunction(uploadLogsToCloud);

    const auto query = @{
        (id) kSecClass: (id) kSecClassGenericPassword,
        (id) kSecAttrAccount: valueTag,
        (id) kSecValueData: data,
        (id) kSecAttrAccessGroup: kAccessGroup,
        (id) kSecAttrAccessible: (id) kSecAttrAccessibleAfterFirstUnlock};

    SecItemDelete((__bridge CFDictionaryRef) query);
    const auto status = SecItemAdd((__bridge CFDictionaryRef) query, nullptr);
    if (status == errSecSuccess)
        log(logTag, @"keychain data was stored successfully.");
    else
        log(logTag, [NSString stringWithFormat: @"can't store keychain data, status is %d", status]);
}

NSMutableDictionary* dataToDictionary(NSData* data, const NSString* logTag)
{
    if (!data)
        return nil;

    NSError* parseError = nil;
    NSMutableDictionary* values = [NSJSONSerialization JSONObjectWithData: data
        options: NSJSONReadingMutableContainers error: &parseError];

    if (!parseError && values)
        return values;

    Logger::log(logTag, @"can't parse access token data.");
    return nil;
}

} // namespace

namespace nx::vms::client::mobile::details {

void PushIpcData::storeImpl(const std::string& user, const std::string& cloudRefreshToken)
{
    static const NSString* kLogTag = @"store credentials";

    // The second part held the password for servers older than 5.0. It is left empty, so the data
    // keeps the layout which older versions use.
    const auto buffer = user + kDelimiter + kDelimiter + cloudRefreshToken;
    const auto data = [[NSString stringWithUTF8String: buffer.c_str()]
        dataUsingEncoding: NSUTF8StringEncoding];

    saveDataForTag(kPushCredentialsTag, data, kLogTag);
}

bool PushIpcData::load(std::string& user, std::string& cloudRefreshToken)
{
    static const NSString* kLogTag = @"load credentials";
    user = std::string();
    cloudRefreshToken = std::string();

    const NSData* data = loadDataForTag(kPushCredentialsTag, kLogTag);
    if (!data)
        return false;

    std::istringstream stream(
        std::string(static_cast<const char*>([data bytes]), [data length]));
    std::vector<std::string> parts;
    std::string tmp;
    while(getline(stream, tmp, kDelimiter))
        parts.push_back(tmp);

    static constexpr int kMinDataPartsCount = 2; //< User + the unused password.
    if (parts.size() < kMinDataPartsCount)
    {
        Logger::log(kLogTag, @"invalid data parts count.");
        return false;
    }

    user = parts[0];

    cloudRefreshToken = parts.size() > 2 //< Support for the cloud refresh token field.
        ? parts[2]
        : std::string();

    Logger::log(kLogTag, @"keychain data was loaded.");
    return true;
}

void PushIpcData::clear()
{
    const auto query = @{
        (id) kSecClass: (id) kSecClassGenericPassword,
        (id) kSecAttrAccount: kPushCredentialsTag,
        (id) kSecAttrAccessGroup: kAccessGroup,
        (id) kSecAttrAccessible: (id) kSecAttrAccessibleAfterFirstUnlock};

    SecItemDelete((__bridge CFDictionaryRef) query);
    Logger::log(@"clear credentials", @"keychain data was cleaned up.");
}

PushIpcData::TokenInfo PushIpcData::accessToken(
    const std::string& cloudSystemId,
    const std::string& user)
{
    static const NSString* kLogTag = @"load access token";
    if (cloudSystemId.empty() || user.empty())
        return {"", {}};

    // Saving deletes the keychain item and adds it again, so reading must not happen in between.
    const std::lock_guard<std::mutex> lock(accessTokensMutex);

    NSData* data = loadDataForTag(kAccessTokensTag, kLogTag);
    if (!data)
        return {"", {}};

    NSDictionary* values = dataToDictionary(data, kLogTag);
    if (!values)
        return {"", {}};

    const NSDictionary* tokenInfo =
        [values objectForKey: [NSString stringWithUTF8String: cloudSystemId.c_str()]];

    if (![tokenInfo isKindOfClass:[NSDictionary class]])
        return {"", {}};

    const NSString* token = [tokenInfo objectForKey: @"accessToken"];
    const NSNumber* expiresAt = [tokenInfo objectForKey: @"expiresAt"];

    if (!token || ![token length] || !expiresAt)
    {
        Logger::log(kLogTag, @"token was not found.");
        return {"", {}};
    }

    // The token is issued for one user and must not be used by another one.
    const NSString* tokenUser = [tokenInfo objectForKey: @"user"];
    if (![tokenUser isKindOfClass: [NSString class]]
        || [tokenUser caseInsensitiveCompare: [NSString stringWithUTF8String: user.c_str()]]
            != NSOrderedSame)
    {
        Logger::log(kLogTag, @"token owner does not match.");
        return {"", {}};
    }

    const std::chrono::milliseconds expiresAtMs([expiresAt longLongValue]);
    if (expiresAtMs <= std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()))
    {
        Logger::log(kLogTag, @"token has expired.");
        return {"", {}};
    }

    Logger::log(kLogTag, @"token was loaded.");
    return {std::string([token UTF8String]), expiresAtMs};
}

void PushIpcData::setAccessToken(
    const std::string& cloudSystemId,
    const std::string& user,
    const std::string& accessToken,
    const std::chrono::milliseconds& expiresAt)
{
    static const NSString* kLogTag = @"set access token";

    // An empty token must not remove the stored one, and a token without the user can't be read.
    if (cloudSystemId.empty() || user.empty() || accessToken.empty())
        return;

    const std::lock_guard<std::mutex> lock(accessTokensMutex);

    NSData* data = loadDataForTag(kAccessTokensTag, kLogTag);

    bool newlyCreated = false;
    NSMutableDictionary* values = dataToDictionary(data, kLogTag);
    if (!values)
    {
        newlyCreated = true;
        values = [[NSMutableDictionary alloc] init];
    }

    // Drop expired tokens of all systems. Otherwise the storage grows with every new system.
    const long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    NSMutableArray* expiredKeys = [NSMutableArray array];
    for (NSString* systemId in values)
    {
        const NSDictionary* systemTokenInfo = [values objectForKey: systemId];
        const NSNumber* systemExpiresAt = [systemTokenInfo isKindOfClass: [NSDictionary class]]
            ? [systemTokenInfo objectForKey: @"expiresAt"]
            : nil;

        if (![systemExpiresAt isKindOfClass: [NSNumber class]]
            || [systemExpiresAt longLongValue] <= nowMs)
        {
            [expiredKeys addObject: systemId];
        }
    }

    if ([expiredKeys count] > 0)
    {
        [values removeObjectsForKeys: expiredKeys];
        Logger::log(kLogTag, [NSString stringWithFormat:
            @"%lu expired tokens were removed.", (unsigned long) [expiredKeys count]]);
    }

    NSString* key = [NSString stringWithUTF8String: cloudSystemId.c_str()];
    NSDictionary* tokenInfo = [NSDictionary dictionaryWithObjectsAndKeys:
        [NSString stringWithUTF8String: accessToken.c_str()], @"accessToken",
        [NSNumber numberWithLong: expiresAt.count()], @"expiresAt",
        [NSString stringWithUTF8String: user.c_str()], @"user", nil];

    [values setObject: tokenInfo forKey: key];

    NSError* error = nil;
    NSData* targetData =
        [NSJSONSerialization dataWithJSONObject: values options: 0 error: &error];

    if (newlyCreated)
        [values release];

    if (error)
    {
        Logger::log(kLogTag, @"can't serialize access token data.");
        return;
    }

    saveDataForTag(kAccessTokensTag, targetData, kLogTag);
}

void PushIpcData::removeAccessToken(
    const std::string& cloudSystemId,
    const std::string& accessToken)
{
    static const NSString* kLogTag = @"removing access token";
    if (cloudSystemId.empty() || accessToken.empty())
        return;

    const std::lock_guard<std::mutex> lock(accessTokensMutex);

    NSData* data = loadDataForTag(kAccessTokensTag, kLogTag);
    NSMutableDictionary* values = dataToDictionary(data, kLogTag);
    NSString* key = [NSString stringWithUTF8String: cloudSystemId.c_str()];
    const NSDictionary* tokenInfo = [values objectForKey: key];
    const NSString* storedToken = [tokenInfo isKindOfClass: [NSDictionary class]]
        ? [tokenInfo objectForKey: @"accessToken"]
        : nil;

    // Another token may have been saved after this one was loaded. It must be kept.
    if (![storedToken isKindOfClass: [NSString class]]
        || ![storedToken isEqualToString: [NSString stringWithUTF8String: accessToken.c_str()]])
    {
        Logger::log(kLogTag, @"token was already replaced, nothing to do.");
        return;
    }

    [values removeObjectForKey: key];

    NSError* error = nil;
    NSData* targetData =
        [NSJSONSerialization dataWithJSONObject: values options: 0 error: &error];

    if (error)
    {
        Logger::log(kLogTag, @"can't serialize access token data.");
        return;
    }

    saveDataForTag(kAccessTokensTag, targetData, kLogTag);
    Logger::log(kLogTag, @"token was removed.");
}

void PushIpcData::clearAccessTokens()
{
    const std::lock_guard<std::mutex> lock(accessTokensMutex);

    const auto query = @{
        (id) kSecClass: (id) kSecClassGenericPassword,
        (id) kSecAttrAccount: kAccessTokensTag,
        (id) kSecAttrAccessGroup: kAccessGroup,
        (id) kSecAttrAccessible: (id) kSecAttrAccessibleAfterFirstUnlock};

    SecItemDelete((__bridge CFDictionaryRef) query);
    Logger::log(@"clear access tokens", @"keychain data was cleaned up.");
}

bool PushIpcData::cloudLoggerOptions(
    std::string& logSessionId,
    std::chrono::milliseconds& sessionEndTime)
{
    // Do not upload logs from this function to the cloud to prevent a recursion.

    static const NSString* kLogTag = @"load cloud logger options";
    logSessionId = {};
    sessionEndTime = std::chrono::milliseconds::zero();

    const NSData* data =
        loadDataForTag(kCloudLoggerOptionTag, kLogTag, /*uploadLogsToCloud*/ false);

    if (!data)
        return false;

    std::istringstream stream(
        std::string(static_cast<const char*>([data bytes]), [data length]));
    std::vector<std::string> parts;
    std::string tmp;
    while(getline(stream, tmp, kDelimiter))
        parts.push_back(tmp);

    static constexpr int kMinDataPartsCount = 2; //< session id + end time.
    if (parts.size() < kMinDataPartsCount)
    {
        Logger::logToSystemOnly(kLogTag, @"invalid data parts count.");
        return false;
    }

    logSessionId = parts[0];
    try
    {
        sessionEndTime = std::chrono::milliseconds(std::stoll(parts[1]));
    }
    catch(std::exception )
    {
        Logger::logToSystemOnly(kLogTag, @"can't parse endTimeMs parameter.");
        return false;
    }

    Logger::logToSystemOnly(kLogTag, @"cloud login data was loaded.");
    return true;
}

void PushIpcData::setCloudLoggerOptions(
    const std::string& logSessionId,
    const std::chrono::milliseconds& sessionEndTime)
{
    static const NSString* kLogTag = @"set cloud logger options";

    const auto buffer = logSessionId + kDelimiter + std::to_string(sessionEndTime.count());
    const auto data = [[NSString stringWithUTF8String: buffer.c_str()]
        dataUsingEncoding: NSUTF8StringEncoding];

    // Do not upload logs to the cloud to prevent a recursion.
    saveDataForTag(kCloudLoggerOptionTag, data, kLogTag, /*uploadLogsToCloud*/ false);
}

} // namespace nx::vms::client::mobile::details
