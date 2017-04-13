#include <cstdint>
#include <nx/network/websocket/websocket_serializer.h>

namespace nx {
namespace network {
namespace websocket {

namespace {

int calcHeaderSize(bool masked, int payloadLenType)
{
    int result = 2;
    result += masked ? 4 : 0;
    result += payloadLenType <= 125 ? 0 : payloadLenType == 126 ? 2 : 8;

    return result;
}

int fillHeader(char* data, bool fin, int opCode, int payloadLenType, int payloadLen, bool masked, unsigned int mask)
{
    data[0] |= (int)fin << 7;
    data[0] |= opCode & 0xf;
    data[1] |= (int)masked << 7;
    data[1] |= payloadLenType & 0x7f;

    int maskOffset = 2;
    if (payloadLenType == 126)
    {
        *reinterpret_cast<unsigned short*>(data + 2) = htons(payloadLen);
        maskOffset = 4;
    }
    else
    {
        *reinterpret_cast<uint64_t*>(data + 2) = htonll(payloadLen);
        maskOffset = 10;
    }

    if (masked)
        *reinterpret_cast<unsigned int*>(data + maskOffset) = mask;

    return maskOffset;
}

int payloadLenTypeByLen(int64_t len)
{
    if (len <= 125)
        return len;
    else if (len < 65536)
        return 126;
    else
        return 127;
}

}

int prepareFrame(const char* payload, int payloadLen, FrameType type,
    bool fin, bool masked, unsigned int mask, char* out, int outLen)
{
    int payloadLenType = payloadLenTypeByLen(payloadLen);
    int neededOutLen = payloadLen + calcHeaderSize(masked, payloadLenType);
    if (outLen < neededOutLen)
        return neededOutLen;

    int headerOffset = fillHeader(out, fin, type, payloadLenType, payloadLen, masked, mask);
    memcpy(out + headerOffset, payload, payloadLen);

    if (masked)
    {
        for (int i = 0; i < outLen; i++)
            out[i + headerOffset] = out[i + headerOffset] ^ ((unsigned char*)(&mask))[i % 4];
    }

    return neededOutLen;
}

}
}
}