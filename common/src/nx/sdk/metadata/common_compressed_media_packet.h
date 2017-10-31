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
    virtual const int dataSize() const { return m_data.size(); }
    virtual const char* data() const { return m_data.empty() ? nullptr : &m_data[0]; }
    virtual const AbstractMediaContext* context() const { return nullptr; }
    virtual int64_t timestampUsec() const { return m_timestampUsec; }

    void setCodec(const std::string& value) { m_codec = value; }
    void setData(std::vector<char> data) { m_data = std::move(data); }
    void setTimestampUsec(int64_t value) { m_timestampUsec = value; }
private:
    std::vector<char> m_data;
    std::string m_codec;
    int64_t m_timestampUsec = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
