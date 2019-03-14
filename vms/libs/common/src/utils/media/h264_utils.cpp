#include "h264_utils.h"

#include "core/resource/param.h"
#include "utils/media/nalUnits.h"

namespace nx {
namespace media_utils {

//dishonorably stolen from libavcodec source
#ifndef AV_RB16
#   define AV_RB16(x)                           \
    ((((const uint8_t*)(x))[0] << 8) |          \
      ((const uint8_t*)(x))[1])
#endif

// TODO: Code duplication with "h264_utils.cpp".
/**
 * @param data data->context should not be null.
 */
void readH264NALUsFromExtraData(
    const QnConstCompressedVideoDataPtr& data,
    std::vector<std::pair<const quint8*, size_t>>* const nalUnits)
{
    NX_ASSERT(data->context);
    const unsigned char* p = data->context->getExtradata();

    //sps & pps is in the extradata, parsing it...
    //following code has been taken from libavcodec/h264.c

    // prefix is unit len
    //const int reqUnitSize = (data->context->ctx()->extradata[4] & 0x03) + 1;
    /* sps and pps in the avcC always have length coded with 2 bytes,
    * so put a fake nal_length_size = 2 while parsing them */
    //int nal_length_size = 2;

    // Decode sps from avcC
    int cnt = *(p + 5) & 0x1f; // Number of sps
    p += 6;

    for (int i = 0; i < cnt; i++)
    {
        const int nalsize = AV_RB16(p);
        p += 2; //skipping nalusize
        if (nalsize > data->context->getExtradataSize() - (p - data->context->getExtradata()))
            break;
        nalUnits->emplace_back((const quint8*)p, nalsize);
        p += nalsize;
    }

    // Decode pps from avcC
    cnt = *(p++); // Number of pps
    for (int i = 0; i < cnt; ++i)
    {
        const int nalsize = AV_RB16(p);
        p += 2;
        if (nalsize > data->context->getExtradataSize() - (p - data->context->getExtradata()))
            break;
        nalUnits->emplace_back((const quint8*)p, nalsize);
        p += nalsize;
    }
}

void readNALUsFromAnnexBStream(
    const QnConstCompressedVideoDataPtr& data,
    std::vector<std::pair<const quint8*, size_t>>* const nalUnits)
{
    const quint8* dataStart = reinterpret_cast<const quint8*>(data->data());
    const quint8* dataEnd = dataStart + data->dataSize();
    const quint8* naluEnd = nullptr;
    for (const quint8
        *curNalu = NALUnit::findNALWithStartCodeEx(dataStart, dataEnd, &naluEnd),
        *nextNalu = NULL;
        curNalu < dataEnd;
        curNalu = nextNalu)
    {
        nextNalu = NALUnit::findNALWithStartCodeEx(curNalu, dataEnd, &naluEnd);
        NX_ASSERT(nextNalu > curNalu);
        //skipping leading_zero_8bits and trailing_zero_8bits
        while ((naluEnd > curNalu) && (*(naluEnd - 1) == 0))
            --naluEnd;
        if (naluEnd > curNalu)
            nalUnits->emplace_back((const quint8*)curNalu, naluEnd - curNalu);
    }
}

namespace h264 {

bool extractSps(const QnConstCompressedVideoDataPtr& videoData, SPSUnit& sps)
{
    auto nalUnits = decodeNalUnits(videoData);

    for (const auto& nalu: nalUnits)
    {
        if ((*nalu.first & 0x1f) == nuSPS)
        {
            if (nalu.second < 4)
                continue;   //invalid sps
            sps.decodeBuffer(nalu.first, nalu.first + nalu.second);
            return sps.deserialize() == 0;
        }
    }
    return false;
}

bool isH264SeqHeaderInExtraData(const QnConstCompressedVideoDataPtr& data)
{
    return data->context &&
        data->context->getExtradataSize() >= 7 &&
        data->context->getExtradata()[0] == 1;
}

std::vector<std::pair<const quint8*, size_t>> decodeNalUnits(
    const QnConstCompressedVideoDataPtr& videoData)
{
    std::vector<std::pair<const quint8*, size_t>> nalUnits;
    if (isH264SeqHeaderInExtraData(videoData))
        readH264NALUsFromExtraData(videoData, &nalUnits);
    else
        readNALUsFromAnnexBStream(videoData, &nalUnits);
    return nalUnits;
}

} // namespace h264

} // namespace media_utils
} // namespace nx
