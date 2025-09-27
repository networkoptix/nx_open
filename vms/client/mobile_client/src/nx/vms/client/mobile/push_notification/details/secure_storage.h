// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <any>
#include <optional>
#include <string>
#include <vector>

namespace nx::vms::client::mobile {

/**
 * Secure storage bridge for iOS (secure_storage.mm) and Android (secure_storage_android.cpp).
 */
class SecureStorage
{
public:
    /**
     * Constructs SecureStorage. Specialization is provided by the implementation.
     */
    template<typename... Args>
    SecureStorage(const Args&...);

    std::optional<std::string> load(const std::string& key) const;
    void save(const std::string& key, const std::string& value);

    std::optional<std::vector<std::byte>> loadImage(const std::string& id) const;
    void removeImage(const std::string& id);

private:
    std::any m_context;
};

} // namespace nx::vms::client::mobile
