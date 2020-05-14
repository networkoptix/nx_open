#pragma once

#include <nx/utils/url.h>

namespace nx::vms_server_plugins::analytics::hanwha {

class ValueTransformer
{
public:
    virtual ~ValueTransformer() = default;
    [[nodiscard]] virtual nx::utils::Url transformUrl(const nx::utils::Url& url) = 0;
    [[nodiscard]] virtual int transformChannelNumber(int channelNumber) = 0;
};

class CameraValueTransformer: public ValueTransformer
{
public:
    [[nodiscard]] virtual nx::utils::Url transformUrl(const nx::utils::Url& url) override;
    [[nodiscard]] virtual int transformChannelNumber(int channelNumber) override;
};

class NvrValueTransformer: public ValueTransformer
{
public:
    NvrValueTransformer(int channelNumber) : m_channelNumber(channelNumber) {};
    [[nodiscard]] virtual nx::utils::Url transformUrl(const nx::utils::Url& url) override;
    [[nodiscard]] virtual int transformChannelNumber(int channelNumber) override;

private:
    int m_channelNumber;
};

} // namespace nx::vms_server_plugins::analytics::hanwha
