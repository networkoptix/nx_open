#include "protocol_detector.h"

#include <nx/utils/log/assert.h>

namespace nx {
namespace network {

std::string toString(ProtocolMatchResult detectionResult)
{
    switch (detectionResult)
    {
        case ProtocolMatchResult::detected:
            return "detected";
        case ProtocolMatchResult::needMoreData:
            return "needMoreData";
        case ProtocolMatchResult::unknownProtocol:
            return "unknownProtocol";
    }

    NX_ASSERT(false);
    return "unsupported value";
}

void PrintTo(const ProtocolMatchResult& val, ::std::ostream* os)
{
    *os << toString(val);
}

//-------------------------------------------------------------------------------------------------

FixedProtocolPrefixRule::FixedProtocolPrefixRule(const std::string& prefix):
    m_prefix(prefix)
{
}

ProtocolMatchResult FixedProtocolPrefixRule::match(const nx::Buffer& buf)
{
    if ((std::size_t) buf.size() < m_prefix.size())
        return ProtocolMatchResult::needMoreData;

    return strncmp(m_prefix.c_str(), buf.constData(), m_prefix.size()) == 0
        ? ProtocolMatchResult::detected
        : ProtocolMatchResult::unknownProtocol;
}

} // namespace network
} // namespace nx
