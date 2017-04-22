#include "p2p_fwd.h"

namespace ec2 {

ApiPersistentIdData PeerNumberInfo::decode(PeerNumberType number) const
{
    return m_shortIdToFullId.value(number);
}

PeerNumberType PeerNumberInfo::encode(
    const ApiPersistentIdData& peer,
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

} // namespace ec2
