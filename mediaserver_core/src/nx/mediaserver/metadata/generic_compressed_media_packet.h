#pragma once

#include <memory>
#include <vector>
#include <string>
#include <cstdint>

#include <plugins/plugin_tools.h>

#include <nx/sdk/metadata/media_context.h>

namespace nx {
namespace sdk {
namespace metadata {

template <class T>
class GenericCompressedMediaPacket: public nxpt::CommonRefCounter<T>
{
public:
    virtual const char* codec() const { return m_codec.c_str(); }

    virtual const int dataSize() const
    {
        return m_ownedData ? (int) m_ownedData->size() : m_externalDataSize;
    }

    virtual const char* data() const
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

    virtual const MediaContext* context() const { return nullptr; }
    virtual int64_t timestampUsec() const { return m_timestampUsec; }
    virtual MediaFlags flags() const {return m_mediaFlags; }

    void setCodec(const std::string& value) { m_codec = value; }
    void setTimestampUsec(int64_t value) { m_timestampUsec = value; }

    void setFlags(MediaFlags flags) { m_mediaFlags = flags; }
    void addFlag(MediaFlag flag) { m_mediaFlags |= (MediaFlags) flag; }
    void removeFlag(MediaFlag flag) { m_mediaFlags &= ~((MediaFlags) flag); }

private:
    std::unique_ptr<std::vector<char>> m_ownedData; //< Deep copy.
    const char* m_externalData = nullptr;
    int m_externalDataSize = 0;
    std::string m_codec;
    int64_t m_timestampUsec = 0;
    MediaFlags m_mediaFlags = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
