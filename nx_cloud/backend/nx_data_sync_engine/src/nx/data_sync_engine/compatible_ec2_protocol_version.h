#pragma once

namespace nx {
namespace data_sync_engine {

class NX_DATA_SYNC_ENGINE_API ProtocolVersionRange
{
public:
    /**
     * Both boundaries are inclusive.
     */
    ProtocolVersionRange(int begin, int end);

    int begin() const;
    int currentVersion() const;
    bool isCompatible(int version) const;

    static const ProtocolVersionRange any;

private:
    int m_begin = 0;
    int m_end = 0;
};

} // namespace data_sync_engine
} // namespace nx
