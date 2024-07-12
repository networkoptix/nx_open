// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/codec/nal_units.h>
#include <nx/utils/bit_stream.h>
#include <nx/utils/log/log.h>

namespace nx::media::nal {

void write_rbsp_trailing_bits(nx::utils::BitStreamWriter& writer)
{
    writer.putBit(1);
    int rest = 8 - (writer.getBitsCount() & 7);
    if (rest == 8)
        return;
    writer.putBits(rest, 0);
}

void write_byte_align_bits(nx::utils::BitStreamWriter& writer)
{
    int rest = 8 - (writer.getBitsCount() & 7);
    if (rest == 8)
        return;
    writer.putBit(1);
    if (rest > 1)
        writer.putBits(rest-1, 0);
}

const uint8_t* findNextNAL(const uint8_t* buffer, const uint8_t* end)
{
    for (buffer += 2; buffer < end;)
    {
        if (*buffer > 1)
            buffer += 3;
        else if (*buffer == 0)
            buffer ++;
        else if (buffer[-2] == 0 && buffer[-1] == 0)    //*buffer == 1
            return buffer+1;
        else
            buffer+=3;
    }
    return end;
}

const uint8_t* findNALWithStartCode(const uint8_t* buffer, const uint8_t* end)
{
    const uint8_t* bufStart = buffer;
    for (buffer += 2; buffer < end;)
    {
        if (*buffer > 1)
            buffer += 3;
        else if (*buffer == 0)
            buffer ++;
        else if (buffer[-2] == 0 && buffer[-1] == 0) {
            if (buffer-3 >= bufStart && buffer[-3] == 0)
                return buffer - 3;
            else
                return buffer-2;
        }
        else
            buffer+=3;
    }
    return end;
}

int encodeEpb(const uint8_t* srcBuffer, const uint8_t* srcEnd, uint8_t* dstBuffer, size_t dstBufferSize)
{
    const uint8_t* srcStart = srcBuffer;
    uint8_t* initDstBuffer = dstBuffer;
    for (srcBuffer += 2; srcBuffer < srcEnd;)
    {
        if (*srcBuffer > 3)
            srcBuffer += 3;
        else if (srcBuffer[-2] == 0 && srcBuffer[-1] == 0) {
            if (dstBufferSize < (size_t) (srcBuffer - srcStart + 2))
                return -1;
            memcpy(dstBuffer, srcStart, srcBuffer - srcStart);
            dstBuffer += srcBuffer - srcStart;
            dstBufferSize -= srcBuffer - srcStart + 2;
            *dstBuffer++ = 3;
            *dstBuffer++ = *srcBuffer++;
            for (int k = 0; k < 1; k++)
                if (srcBuffer < srcEnd) {
                    if (dstBufferSize < 1)
                        return -1;
                    *dstBuffer++ = *srcBuffer++;
                    dstBufferSize--;
                }
            srcStart = srcBuffer;
        }
        else
            srcBuffer++;
    }

    NX_ASSERT(srcEnd >= srcStart);
    //conversion to uint possible cause srcEnd should always be greater then srcStart
    if (dstBufferSize < uint(srcEnd - srcStart))
        return -1;
    memcpy(dstBuffer, srcStart, srcEnd - srcStart);
    dstBuffer += srcEnd - srcStart;
    return dstBuffer - initDstBuffer;
}

int decodeEpb(const uint8_t* srcBuffer, const uint8_t* srcEnd, uint8_t* dstBuffer, size_t dstBufferSize)
{
    uint8_t* initDstBuffer = dstBuffer;
    const uint8_t* srcStart = srcBuffer;
    for (srcBuffer += 3; srcBuffer < srcEnd;)
    {
        if (*srcBuffer > 3)
        {
            srcBuffer += 4;
        }
        else if (srcBuffer[-3] == 0 && srcBuffer[-2] == 0  && srcBuffer[-1] == 3)
        {
            if (dstBufferSize < (size_t) (srcBuffer - srcStart))
                return -1;

            memcpy(dstBuffer, srcStart, srcBuffer - srcStart - 1);
            dstBuffer += srcBuffer - srcStart - 1;
            dstBufferSize -= srcBuffer - srcStart;
            *dstBuffer++ = *srcBuffer++;
            srcStart = srcBuffer;
        }
        else
        {
            srcBuffer++;
        }
    }

    if (dstBufferSize < std::size_t(srcEnd - srcStart))
        return -2;

    memcpy(dstBuffer, srcStart, srcEnd - srcStart);
    dstBuffer += srcEnd - srcStart;
    return dstBuffer - initDstBuffer;
}

QByteArray dropBorderedStartCodes(const QByteArray& sourceNal)
{
    QByteArray resutltNal = sourceNal;
    if (resutltNal.endsWith(kStartCode))
        resutltNal.chop(kStartCode.size());
    else if (resutltNal.endsWith(kStartCode3B))
        resutltNal.chop(kStartCode3B.size());

    if (resutltNal.startsWith(kStartCode))
        resutltNal.remove(0, kStartCode.size());
    else if (resutltNal.startsWith(kStartCode3B))
        resutltNal.remove(0, kStartCode3B.size());

    return resutltNal;
}

std::vector<NalUnitInfo> findNalUnitsMp4(const uint8_t* data, int32_t size)
{
    std::vector<NalUnitInfo> result;
    const uint8_t* current = data;
    while (current + 4 < data + size)
    {
        uint32_t naluSize = ntohl(*(uint32_t*)current);
        if (naluSize > uint32_t(data + size - current))
        {
            NX_WARNING(NX_SCOPE_TAG, "Invalid NAL unit size: %1", naluSize);
            break;
        }
        result.emplace_back(NalUnitInfo{current + kNalUnitSizeLength, (int)naluSize});
        current += kNalUnitSizeLength + naluSize;
    }
    return result;
}

std::vector<NalUnitInfo> findNalUnitsAnnexB(
    const uint8_t* data, int32_t size, bool droppedFirstStartcode)
{
    std::vector<NalUnitInfo> result;
    const uint8_t* dataEnd = data + size;
    const uint8_t* naluStart = droppedFirstStartcode ? data : findNextNAL(data, dataEnd);
    const uint8_t* nextNaluStart = nullptr;
    while (naluStart < dataEnd)
    {
        nextNaluStart = findNextNAL(naluStart, dataEnd);
        const uint8_t *naluEnd = nextNaluStart == dataEnd ? dataEnd : nextNaluStart - 3;

        // skipping leading_zero_8bits and trailing_zero_8bits
        while (naluEnd > naluStart && naluEnd[-1] == 0)
            --naluEnd;

        if (naluEnd > naluStart)
            result.emplace_back(NalUnitInfo{naluStart, int(naluEnd - naluStart)});

        naluStart = nextNaluStart;
    }
    return result;
}

std::vector<uint8_t> convertStartCodesToSizes(const uint8_t* data, int32_t size, int32_t padding)
{
    constexpr int kNaluSizeLenght = 4;
    std::vector<uint8_t> result;
    auto nalUnits = findNalUnitsAnnexB(data, size);
    if (nalUnits.empty())
        return result;

    int32_t resultSize = padding;
    for (const auto& nalu: nalUnits)
        resultSize += nalu.size + kNaluSizeLenght;

    result.resize(resultSize, 0);
    uint8_t* resultData = result.data();
    for (const auto& nalu: nalUnits)
    {
        uint32_t* ptr = (uint32_t*)resultData;
        *ptr = htonl(nalu.size);
        memcpy(resultData + kNaluSizeLenght, nalu.data, nalu.size);
        resultData += nalu.size + kNaluSizeLenght;
    }
    return result;
}

template <size_t arraySize>
bool startsWith(const void* data, size_t size, const std::array<uint8_t, arraySize> value)
{
    return size >= value.size() && memcmp(data, value.data(), value.size()) == 0;
}

bool isStartCode(const void* data, size_t size)
{
    return startsWith(data, size, kStartCode) || startsWith(data, size, kStartCode3B);
}

void convertToStartCodes(uint8_t* const data, const int size)
{
    uint8_t* offset = data;
    while (offset + 4 < data + size)
    {
        size_t naluSize = ntohl(*(uint32_t*)offset);
        memcpy(offset, kStartCode.data(), kStartCode.size());
        offset += kStartCode.size() + naluSize;
    }
}

} // namespace nx::media::nal
