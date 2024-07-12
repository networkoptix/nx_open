// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "common.h"

#include <nx/codec/h264/slice_header.h>
#include <nx/codec/nal_units.h>
#include <nx/utils/bit_stream.h>

namespace {

constexpr int kNalReservedSpace = 16;

}

namespace nx::media::h264 {

NALUnitType decodeType(uint8_t data)
{
    return (NALUnitType)(data & 0x1f);
}

bool isSliceNal(uint8_t nalUnitType)
{
    return nalUnitType >= nuSliceNonIDR && nalUnitType <= nuSliceIDR;
}

int ceil_log2(double val)
{
    int iVal = (int) val;
    if(iVal <= 0)
        return 0;

    double frac = val - iVal;
    int bits = 0;
    for(;iVal > 0; iVal>>=1) {
        bits++;
    }
    int mask = 1 << (bits-1);
    iVal = (int) val;
    if (iVal - mask == 0 && frac == 0)
        return bits-1; // For example: ceil(log2(8.0)) = 3, but for 8.2 or 9.0 is 4
    else
        return bits;
}

bool isIFrame(const uint8_t* data, int dataLen)
{
    if (dataLen < 2)
        return false;

    const auto nalType = decodeType(*data);
    bool isSlice = isSliceNal(nalType);
    if (!isSlice)
        return false;

    if (nalType == nuSliceIDR)
        return true;

    nx::utils::BitStreamReader bitReader;
    bitReader.setBuffer(data + 1, data + dataLen);
    try
    {
        //extract first_mb_in_slice
        bitReader.getGolomb();

        int slice_type = bitReader.getGolomb();
        if (slice_type >= 5)
            slice_type -= 5; // +5 flag is: all other slice at this picture must be same type

        return (slice_type == SliceHeader::I_TYPE || slice_type == SliceHeader::SI_TYPE);
    }
    catch (...)
    {
        return false;
    }
}

int NALUnit::deserialize(uint8_t* buffer, uint8_t* end)
{
    if (end == buffer)
        return NOT_ENOUGHT_BUFFER;

    //NX_ASSERT((*buffer & 0x80) == 0);
    if ((*buffer & 0x80) != 0) {
        qWarning() << "Invalid forbidden_zero_bit for nal unit " << decodeType(*buffer);
    }

    nal_ref_idc   = (*buffer >> 5) & 0x3;
    nal_unit_type = decodeType(*buffer);
    return 0;
}

void NALUnit::decodeBuffer(const uint8_t* buffer, const uint8_t* end)
{
    delete [] m_nalBuffer;
    m_nalBuffer = new uint8_t[end - buffer + kNalReservedSpace];
    m_nalBufferLen = nx::media::nal::decodeEpb(buffer, end, m_nalBuffer, end - buffer);
}

int NALUnit::serializeBuffer(uint8_t* dstBuffer, uint8_t* dstEnd, bool writeStartCode) const
{
    if (m_nalBufferLen == 0)
        return 0;
    if (writeStartCode)
    {
        if (dstEnd - dstBuffer < 4)
            return -1;
        *dstBuffer++ = 0;
        *dstBuffer++ = 0;
        *dstBuffer++ = 0;
        *dstBuffer++ = 1;
    }
    int encodeRez = nx::media::nal::encodeEpb(m_nalBuffer, m_nalBuffer + m_nalBufferLen, dstBuffer, dstEnd - dstBuffer);
    if (encodeRez == -1)
        return -1;
    else
        return encodeRez + (writeStartCode ? 4 : 0);
}

int NALUnit::serialize(uint8_t* dstBuffer)
{
    *dstBuffer++ = 0;
    *dstBuffer++ = 0;
    *dstBuffer++ = 1;
    *dstBuffer = ((nal_ref_idc & 3) << 5) + (nal_unit_type & 0x1f);
    return 4;
}

void NALUnit::scaling_list(int* scalingList, int sizeOfScalingList, bool& useDefaultScalingMatrixFlag)
{
    int lastScale = 8;
    int nextScale = 8;
    for(int j = 0; j < sizeOfScalingList; j++ )
    {
        if( nextScale != 0 ) {
            int delta_scale = bitReader.getSignedGolomb();
            nextScale = ( lastScale + delta_scale + 256 ) % 256;
            useDefaultScalingMatrixFlag = ( j  ==  0 && nextScale  ==  0 );
        }
        scalingList[j] = ( nextScale  ==  0 ) ? lastScale : nextScale;
        lastScale = scalingList[ j ];
    }
}

void NALUnit::updateBits(int bitOffset, int bitLen, int value)
{
    //uint8_t* ptr = m_getbitContextBuffer + (bitOffset/8);
    uint8_t* ptr = (uint8_t*) bitReader.getBuffer() + bitOffset/8;
    nx::utils::BitStreamWriter bitWriter;
    int byteOffset = bitOffset % 8;
    bitWriter.setBuffer(ptr, ptr + (bitLen / 8 + 5));

    uint8_t* ptr_end = (uint8_t*) bitReader.getBuffer() + (bitOffset + bitLen)/8;
    int endBitsPostfix = 8 - ((bitOffset + bitLen) % 8);

    if (byteOffset > 0) {
        int prefix = *ptr >> (8-byteOffset);
        bitWriter.putBits(byteOffset, prefix);
    }
    bitWriter.putBits(bitLen, value);

    if (endBitsPostfix < 8) {
        int postfix = *ptr_end & ( ( 1 << endBitsPostfix) - 1);
        bitWriter.putBits(endBitsPostfix, postfix);
    }
    bitWriter.flushBits();
}

} // namespace nx::media::h264
