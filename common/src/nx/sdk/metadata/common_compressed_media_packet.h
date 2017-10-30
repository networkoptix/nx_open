#pragma once

#include "abstract_data_packet.h"
#include "abstract_media_context.h"

#include <vector>
#include <string>

namespace nx {
namespace sdk {
namespace metadata {

class CommonCompressedMediaPacket
{
public:
    const char* codec() const { return m_codec.c_str();  }
    const int dataSize() const { return m_data.size(); }
    const char* data() const { return m_data.empty() ? nullptr : &m_data[0]; }
    const AbstractMediaContext* context() const { return nullptr; }
    int64_t timestampUsec() const { return m_timestampUsec; }

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
