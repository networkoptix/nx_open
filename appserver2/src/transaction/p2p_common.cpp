#include "p2p_fwd.h"

namespace ec2 {

ApiPersistentIdData PeerNumberInfo::decode(PeerNumberType number) const
{
    return m_shortIdToFullId.value(number);
}

PeerNumberType PeerNumberInfo::encode(const ApiPersistentIdData& peer)
{
    auto itr = m_fullIdToShortId.find(peer);
    if (itr != m_fullIdToShortId.end())
        return itr.value();

    PeerNumberType result = m_fullIdToShortId.size();
    m_fullIdToShortId.insert(peer, result);
    m_shortIdToFullId.insert(result, peer);
    return result;
}

} // namespace ec2
