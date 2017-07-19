#include "p2p_fwd.h"

#include <nx/fusion/serialization/lexical.h>
#include <nx/fusion/serialization/lexical_enum.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::p2p, MessageType);

namespace nx {
namespace p2p {

const char* const kP2pProtoName = "nxp2p";

ec2::ApiPersistentIdData PeerNumberInfo::decode(PeerNumberType number) const
{
    return m_shortIdToFullId.value(number);
}

PeerNumberType PeerNumberInfo::encode(
    const ec2::ApiPersistentIdData& peer,
    PeerNumberType shortNumber)
{
    auto itr = m_fullIdToShortId.find(peer);
    if (itr != m_fullIdToShortId.end())
        return itr.value();

    if (shortNumber == kUnknownPeerNumnber)
        shortNumber = m_fullIdToShortId.size();
    m_fullIdToShortId.insert(peer, shortNumber);
    m_shortIdToFullId.insert(shortNumber, peer);
    return shortNumber;
}

} // namespace p2p
} // namespace nx
