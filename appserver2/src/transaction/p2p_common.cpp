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

template <std::size_t N>
void serializeCompressedValue(BitStreamWriter& writer, quint32 value, const std::array<int, N>& bitsGroups)
{
    for (int i = 0; i < bitsGroups.size(); ++i)
    {
        writer.putBits(bitsGroups[i], value);
        if (i == bitsGroups.size() - 1)
            break; //< 16 bits written

        value >>= bitsGroups[i];
        if (value == 0)
        {
            writer.putBit(0); //< end of number
            break;
        }
        writer.putBit(1); //< number continuation
    }
}

template <std::size_t N>
quint32 deserializeCompressedValue(BitStreamReader& reader, const std::array<int, N>& bitsGroups)
{
    quint32 value = 0;
    int shift = 0;
    for (int i = 0; i < bitsGroups.size(); ++i)
    {
        value += (reader.getBits(bitsGroups[i]) << shift);
        if (i == bitsGroups.size() - 1 || reader.getBit() == 0)
            break;
        shift += bitsGroups[i];
    }
    return value;
}

const static std::array<int, 3> peerNumberbitsGroups = { 7, 3, 4 };

void serializeCompressPeerNumber(BitStreamWriter& writer, PeerNumberType peerNumber)
{
    serializeCompressedValue(writer, peerNumber, peerNumberbitsGroups);
}

PeerNumberType deserializeCompressPeerNumber(BitStreamReader& reader)
{
    return deserializeCompressedValue(reader, peerNumberbitsGroups);
}

const static std::array<int, 4> compressedSizebitsGroups = { 7, 7, 7, 8 };

void serializeCompressedSize(BitStreamWriter& writer, quint32 peerNumber)
{
    serializeCompressedValue(writer, peerNumber, compressedSizebitsGroups);
}

quint32 deserializeCompressedSize(BitStreamReader& reader)
{
    return deserializeCompressedValue(reader, compressedSizebitsGroups);
}

} // namespace ec2
