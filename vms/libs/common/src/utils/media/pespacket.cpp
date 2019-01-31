//#include "stdafx.h"
#include "pespacket.h"

void PESPacket::serialize(quint64 pts, quint8 streamID)
{
    setStreamID(streamID);
    setPacketLength(0);
    resetFlags();
    setHeaderLength(PESPacket::HEADER_SIZE + PESPacket::PTS_SIZE); // 14 bytes
    setPts(pts);
}

void PESPacket::serialize(quint64 pts, quint64 dts, quint8 streamID)
{
    setStreamID(streamID);
    setPacketLength(0);
    setHeaderLength(PESPacket::HEADER_SIZE + PESPacket::PTS_SIZE * 2); // 19 bytes
    resetFlags();
    setPtsAndDts(pts, dts);
}

void PESPacket::serialize(quint8 streamID)
{
    setStreamID(streamID);
    setPacketLength(0);
    setHeaderLength(PESPacket::HEADER_SIZE); // 19 bytes
    resetFlags();
}
