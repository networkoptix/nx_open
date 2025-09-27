// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <Foundation/Foundation.h>
#include <Security/Security.h>

#include <nx/vms/client/mobile/push_notification/details/secure_storage.h>

#include "ipc_access_group.h"
#include "logger.h"

using Logger = extension::Logger;

namespace {

const auto kAccessGroup =
    [NSString stringWithUTF8String: nx::vms::client::mobile::details::kIpcAccessGroup];

const auto kApplicationGroup =
    [NSString stringWithUTF8String: nx::vms::client::mobile::details::kApplicationGroup];

const auto kLogTag = @"secure storage";

} // namespace

namespace nx::vms::client::mobile {

template<>
SecureStorage::SecureStorage()
{
}

std::optional<std::string> SecureStorage::load(const std::string& key) const
{
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
    {
        Logger::log(kLogTag,
            [NSString stringWithFormat: @"Failed to load %d", (int) status]);

        return std::nullopt;
    }

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

std::optional<std::vector<std::byte>> SecureStorage::loadImage(const std::string& id) const
{
    const auto filePath = [NSString stringWithUTF8String: id.c_str()];
    NSData* data = [NSData dataWithContentsOfFile: filePath];

    if (!data)
    {
        Logger::log(kLogTag, [NSString stringWithFormat: @"Failed to load file"]);
        return std::nullopt;
    }

    const auto ptr = static_cast<const std::byte*>([data bytes]);
    return std::vector<std::byte>(ptr, ptr + data.length);
}

void SecureStorage::removeImage(const std::string& id)
{
    const auto filePath = [NSString stringWithUTF8String: id.c_str()];
    const auto fileManager = [NSFileManager defaultManager];

    NSError* error = nil;
    [fileManager removeItemAtPath: filePath error: &error];

    if (error)
    {
        Logger::log(kLogTag,
            [NSString stringWithFormat: @"Failed to remove file %@: %@", filePath, error]);
    }
}

} // namespace nx::vms::client::mobile
