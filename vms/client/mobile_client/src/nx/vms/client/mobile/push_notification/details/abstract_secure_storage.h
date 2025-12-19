// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace nx::vms::client::mobile {

/**
 * Interface for platform-specific secure storage.
 */
class AbstractSecureStorage
{
public:
    virtual ~AbstractSecureStorage() = default;

    virtual std::optional<std::string> load(const std::string& key) const = 0;
    virtual void save(const std::string& key, const std::string& value) = 0;

    virtual std::optional<std::vector<std::byte>> loadFile(const std::string& key) const = 0;
    virtual void saveFile(const std::string& key, const std::vector<std::byte>& data) = 0;
    virtual void removeFile(const std::string& key) = 0;
};

} // namespace nx::vms::client::mobile
