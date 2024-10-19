// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <functional>
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

    const auto data = (__bridge NSData*) result;

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

void PushIpcData::store(
    const std::string& user,
    const std::string& cloudRefreshToken,
    const std::string& password)
{
    static const NSString* kLogTag = @"store credentials";
    const auto buffer = user + kDelimiter + password + kDelimiter + cloudRefreshToken;
    const auto data = [[NSString stringWithUTF8String: buffer.c_str()]
        dataUsingEncoding: NSUTF8StringEncoding];

    saveDataForTag(kPushCredentialsTag, data, kLogTag);
}

bool PushIpcData::load(
    std::string& user,
    std::string& cloudRefreshToken,
    std::string& password)
{
    static const NSString* kLogTag = @"load credentials";
    user = std::string();
    cloudRefreshToken = std::string();
    password = std::string();

    const NSData* data = loadDataForTag(kPushCredentialsTag, kLogTag);
    if (!data)
        return false;

    std::istringstream stream(static_cast<const char*>([data bytes]));
    std::vector<std::string> parts;
    std::string tmp;
    while(getline(stream, tmp, kDelimiter))
        parts.push_back(tmp);

    static constexpr int kMinDataPartsCount = 2; //< User + password
    if (parts.size() < kMinDataPartsCount)
    {
        Logger::log(kLogTag, @"invalid data parts count.");
        return false;
    }

    user = parts[0];
    password = parts[1];

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
    const std::string& cloudSystemId)
{
    static const NSString* kLogTag = @"load access token";
    if (cloudSystemId.empty())
        return {"", {}};

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

    if (token && [token length] && expiresAt)
    {
        Logger::log(kLogTag, @"token was loaded.");
        return {std::string([token UTF8String]), std::chrono::milliseconds([expiresAt longValue])};
    }

    Logger::log(kLogTag, @"token was not found.");
    return {"", {}};
}

void PushIpcData::setAccessToken(
    const std::string& cloudSystemId,
    const std::string& accessToken,
    const std::chrono::milliseconds& expiresAt)
{
    static const NSString* kLogTag = @"set access token";
    if (cloudSystemId.empty())
        return;

    NSData* data = loadDataForTag(kAccessTokensTag, kLogTag);

    bool newlyCreated = false;
    NSMutableDictionary* values = dataToDictionary(data, kLogTag);
    if (!values)
    {
        newlyCreated = true;
        values = [[NSMutableDictionary alloc] init];
    }

    NSString* key = [NSString stringWithUTF8String: cloudSystemId.c_str()];
    if (accessToken.empty())
    {
        [values removeObjectForKey: key];
    }
    else
    {
        NSDictionary* tokenInfo = [NSDictionary dictionaryWithObjectsAndKeys:
            [NSString stringWithUTF8String: accessToken.c_str()], @"accessToken",
            [NSNumber numberWithLong: expiresAt.count()], @"expiresAt", nil];

        [values setObject: tokenInfo forKey: key];
    }

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

void PushIpcData::removeAccessToken(const std::string& cloudSystemId)
{
    static const NSString* kLogTag = @"removing access token";
    Logger::log(kLogTag, @"started.");
    setAccessToken(cloudSystemId, {}, {});
    Logger::log(kLogTag, @"finished.");
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

    std::istringstream stream(static_cast<const char*>([data bytes]));
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
