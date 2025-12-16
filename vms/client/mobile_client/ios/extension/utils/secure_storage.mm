// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <Foundation/Foundation.h>
#include <Security/Security.h>

#include <nx/vms/client/mobile/push_notification/details/secure_storage.h>
#include <nx/vms/client/mobile/push_notification/details/utils.h>

#include "ipc_access_group.h"
#include "logger.h"

using Logger = extension::Logger;

namespace {

const auto kAccessGroup =
    [NSString stringWithUTF8String: nx::vms::client::mobile::details::kIpcAccessGroup];

const auto kApplicationGroup =
    [NSString stringWithUTF8String: nx::vms::client::mobile::details::kApplicationGroup];

const auto kLogTag = @"secure storage";

NSURL* fileUrl(const std::string& key)
{
    const auto fileManager = [NSFileManager defaultManager];
    const auto sharedContainerUrl =
        [fileManager containerURLForSecurityApplicationGroupIdentifier: kApplicationGroup];
    const auto directory =
        [sharedContainerUrl URLByAppendingPathComponent: @"secure_storage" isDirectory: YES];
    const auto fileName = [NSString stringWithFormat: @"%llu", hash(key)];
    return [directory URLByAppendingPathComponent: fileName];
}

void remove(NSString* key)
{
    const auto query = @{
        (id) kSecClass: (id) kSecClassGenericPassword,
        (id) kSecAttrAccount: key,
        (id) kSecAttrAccessGroup: (id) kAccessGroup,
        (id) kSecAttrAccessible: (id) kSecAttrAccessibleAfterFirstUnlock
    };

    SecItemDelete((__bridge CFDictionaryRef) query);
}

bool isActual(NSString* key)
{
    return [[NSUserDefaults standardUserDefaults] boolForKey: key];
}

void setActual(NSString* key, bool value)
{
    [[NSUserDefaults standardUserDefaults] setBool: value forKey: key];
}

/**
 * Ensures that Keychain is empty when Client is reinstalled, since Keychain is persistent. Uses
 * NSUserDefaults flags, which are not preserved, as indication that Client has been reinstalled.
 */
void ensureActual(NSString* key)
{
    if (isActual(key))
        return;

    remove(key);
    setActual(key, true);
}

} // namespace

namespace nx::vms::client::mobile {

template<>
SecureStorage::SecureStorage()
{
}

std::optional<std::string> SecureStorage::load(const std::string& key) const
{
    ensureActual([NSString stringWithUTF8String: key.c_str()]);

    const auto query = @{
        (id) kSecClass: (id) kSecClassGenericPassword,
        (id) kSecAttrAccount: [NSString stringWithUTF8String: key.c_str()],
        (id) kSecMatchLimit: (id) kSecMatchLimitOne,
        (id) kSecReturnData: (id) kCFBooleanTrue,
        (id) kSecAttrAccessGroup: (id) kAccessGroup,
        (id) kSecAttrAccessible: (id) kSecAttrAccessibleAfterFirstUnlock
    };

    CFTypeRef resultData = nullptr;
    const auto status = SecItemCopyMatching((__bridge CFDictionaryRef) query, &resultData);
    if (status != errSecSuccess)
        return std::nullopt;

    NSData* data = (__bridge NSData*)(resultData);
    std::string result((const char*) [data bytes], [data length]);
    CFRelease(data);

    return result;
}

void SecureStorage::save(const std::string& key, const std::string& value)
{
    const NSData* data = [NSData dataWithBytes: value.data() length: value.size()];
    const auto query = @{
        (id) kSecClass: (id) kSecClassGenericPassword,
        (id) kSecAttrAccount: [NSString stringWithUTF8String: key.c_str()],
        (id) kSecValueData: data,
        (id) kSecAttrAccessGroup: (id) kAccessGroup,
        (id) kSecAttrAccessible: (id) kSecAttrAccessibleAfterFirstUnlock
    };

    SecItemDelete((__bridge CFDictionaryRef) query);

    const auto status = SecItemAdd((__bridge CFDictionaryRef) query, nullptr);

    if (status != errSecSuccess)
        Logger::log(kLogTag, [NSString stringWithFormat: @"Failed to save %d", (int) status]);
}

std::optional<std::vector<std::byte>> SecureStorage::loadFile(const std::string& key) const
{
    const auto data = [NSData dataWithContentsOfURL: fileUrl(key)];

    if (!data)
    {
        Logger::log(kLogTag, [NSString stringWithFormat: @"Failed to load file"]);
        return std::nullopt;
    }

    const auto ptr = static_cast<const std::byte*>([data bytes]);
    return std::vector<std::byte>(ptr, ptr + data.length);
}

void SecureStorage::saveFile(const std::string& key, const std::vector<std::byte>& data)
{
    const auto url = fileUrl(key);
    const auto directory = [url URLByDeletingLastPathComponent];
    const auto fileManager = [NSFileManager defaultManager];
    [fileManager createDirectoryAtURL: directory
        withIntermediateDirectories: true attributes: nil error: nil];

    const auto result = [NSData dataWithBytes: data.data() length: data.size()];

    if (![result writeToURL: url atomically: true])
        Logger::log(kLogTag, @"Failed to save file");
}

void SecureStorage::removeFile(const std::string& key)
{
    const auto url = fileUrl(key);
    const auto fileManager = [NSFileManager defaultManager];

    NSError* error = nil;
    [fileManager removeItemAtURL: url error: &error];

    if (error)
        Logger::log(kLogTag, [NSString stringWithFormat: @"Failed to remove file: %@", error]);
}

} // namespace nx::vms::client::mobile
