#pragma once

#include <optional>

#include <nx/network/buffer.h>
#include <nx/vms/server/resource/resource_fwd.h>

namespace nx::vms::server::plugins {

class HanwhaDefaultPasswordApi
{
public:
    struct Info
    {
        bool isInitialized = false;
        nx::Buffer publicKey;
    };

public:
    HanwhaDefaultPasswordApi(HanwhaResource* device);

    std::optional<Info> fetchInfo();

    bool setPassword(const QString& password);

private:
    HanwhaResource* m_device = nullptr;
};

} // namespace nx::vms::server::plugins
