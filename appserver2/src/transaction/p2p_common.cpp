#include "p2p_fwd.h"
#include <array>

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

void serializeCompressPeerNumber(BitStreamWriter& writer, PeerNumberType peerNumber)
{
    const static std::array<int, 3> bitsGroups = { 7, 3, 4 };
    const static int mask = 0x3fff;  //< only low 14 bits are supported
    NX_ASSERT(peerNumber <= mask);
    peerNumber &= mask;
    for (int i = 0; i < bitsGroups.size(); ++i)
    {
        writer.putBits(bitsGroups[i], peerNumber);
        if (i == bitsGroups.size() - 1)
            break; //< 16 bits written

        peerNumber >>= bitsGroups[i];
        if (peerNumber == 0)
        {
            writer.putBit(0); //< end of number
            break;
        }
        writer.putBit(1); //< number continuation
    }
}

PeerNumberType deserializeCompressPeerNumber(BitStreamReader& reader)
{
    const static std::array<int, 3> bitsGroups = { 7, 3, 4 };
    PeerNumberType peerNumber = 0;
    int shift = 0;
    for (int i = 0; i < bitsGroups.size(); ++i)
    {
        peerNumber += (reader.getBits(bitsGroups[i]) << shift);
        if (i == bitsGroups.size() - 1 || reader.getBit() == 0)
            break;
        shift += bitsGroups[i];
    }
    return peerNumber;
}

} // namespace ec2
