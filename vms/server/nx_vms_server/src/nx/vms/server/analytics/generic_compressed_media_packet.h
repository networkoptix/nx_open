#pragma once

#include <memory>
#include <vector>
#include <string>
#include <cstdint>

#include <plugins/plugin_tools.h>

#include <nx/sdk/analytics/i_media_context.h>

namespace nx {
namespace vms::server {
namespace analytics {

template <class T>
class GenericCompressedMediaPacket: public nxpt::CommonRefCounter<T>
{
public:
    using MediaContext = nx::sdk::analytics::IMediaContext;
    using MediaFlag = nx::sdk::analytics::MediaFlag;
    using MediaFlags = nx::sdk::analytics::MediaFlags;

    virtual const char* codec() const override { return m_codec.c_str(); }

    virtual int dataSize() const override
    {
        return m_ownedData ? (int) m_ownedData->size() : m_externalDataSize;
    }

    virtual const char* data() const override
    {
        if (m_ownedData && !m_ownedData->empty())
            return &m_ownedData->at(0);
        return m_externalData;
    }

    void setOwnedData(std::vector<char> data)
    {
        m_ownedData.reset(new std::vector<char>());
        *m_ownedData = std::move(data);
        m_externalData = nullptr;
        m_externalDataSize = 0;
    }

    void setExternalData(const char* data, int size)
    {
        m_externalData = data;
        m_externalDataSize = size;
        m_ownedData.reset();
    }

    virtual const MediaContext* context() const override { return nullptr; }
    virtual int64_t timestampUs() const override { return m_timestampUs; }
    virtual MediaFlags flags() const override { return m_mediaFlags; }

    void setCodec(const std::string& value) { m_codec = value; }
    void setTimestampUs(int64_t value) { m_timestampUs = value; }

    void setFlags(MediaFlags flags) { m_mediaFlags = flags; }
    void addFlag(MediaFlag flag) { m_mediaFlags |= (MediaFlags) flag; }
    void removeFlag(MediaFlag flag) { m_mediaFlags &= ~((MediaFlags) flag); }

private:
    std::unique_ptr<std::vector<char>> m_ownedData; //< Deep copy.
    const char* m_externalData = nullptr;
    int m_externalDataSize = 0;
    std::string m_codec;
    int64_t m_timestampUs = 0;
    MediaFlags m_mediaFlags = 0;
};

} // namespace analytics
} // namespace vms::server
} // namespace nx
