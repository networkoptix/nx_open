#include "compatible_ec2_protocol_version.h"

#include <limits>

namespace nx::clusterdb::engine {

const ProtocolVersionRange ProtocolVersionRange::any(
    0, std::numeric_limits<int>::max());

ProtocolVersionRange::ProtocolVersionRange(int begin, int end):
    m_begin(begin),
    m_end(end)
{
}

int ProtocolVersionRange::begin() const
{
    return m_begin;
}

int ProtocolVersionRange::currentVersion() const
{
    return m_end;
}

bool ProtocolVersionRange::isCompatible(int version) const
{
    return (version >= m_begin) && (version <= m_end);
}

} // namespace nx::clusterdb::engine
