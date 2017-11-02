#pragma once

#include <plugins/plugin_api.h>
#include <plugins/plugin_tools.h>

#include "abstract_data_packet.h"
#include "abstract_media_context.h"

#include <vector>
#include <string>

namespace nx {
namespace sdk {
namespace metadata {

template <class T>
class CommonCompressedMediaPacket: public nxpt::CommonRefCounter<T>
{
public:
    virtual const char* codec() const { return m_codec.c_str();  }

    virtual const int dataSize() const {
        return m_data ? m_data->size() : m_externalDataSize;
    }
    virtual const char* data() const
    {
        if (m_data && !m_data->empty())
            return &m_data->at(0);
        return m_externalData;
    }

    void setData(std::vector<char> data)
    {
        m_data.reset(new std::vector<char>());
        *m_data = std::move(data);
        m_externalData = nullptr;
        m_externalDataSize = 0;
    }
    void setData(const char* data, int size)
    {
        m_externalData = data;
        m_externalDataSize = size;
        m_data.reset();
    }

    virtual const AbstractMediaContext* context() const { return nullptr; }
    virtual int64_t timestampUsec() const { return m_timestampUsec; }

    void setCodec(const std::string& value) { m_codec = value; }
    void setTimestampUsec(int64_t value) { m_timestampUsec = value; }
private:
    std::unique_ptr<std::vector<char>> m_data; //< Deep copy
    const char* m_externalData = nullptr;
    int m_externalDataSize = 0;
    std::string m_codec;
    int64_t m_timestampUsec = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
