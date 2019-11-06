#include "h264_utils.h"

#include "core/resource/param.h"
#include "utils/media/nalUnits.h"

namespace nx::media_utils {

//dishonorably stolen from libavcodec source
#ifndef AV_RB16
#   define AV_RB16(x)                           \
    ((((const uint8_t*)(x))[0] << 8) |          \
      ((const uint8_t*)(x))[1])
#endif

// TODO: Code duplication with "h264_mp4_to_annexb.cpp".
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
    const uint8_t* data,
    int32_t size,
    std::vector<std::pair<const quint8*, size_t>>* const nalUnits)
{
    const uint8_t* dataEnd = data + size;
    const uint8_t* naluEnd = nullptr;
    for (const uint8_t
        *curNalu = NALUnit::findNALWithStartCodeEx(data, dataEnd, &naluEnd),
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

void readNALUsFromAnnexBStream(
    const QnConstCompressedVideoDataPtr& data,
    std::vector<std::pair<const quint8*, size_t>>* const nalUnits)
{
    readNALUsFromAnnexBStream((const uint8_t*)data->data(), data->dataSize(), nalUnits);
}

void convertStartCodesToSizes(const uint8_t* data, int32_t size)
{
    std::vector<std::pair<const quint8*, size_t>> nalUnits;
    readNALUsFromAnnexBStream(data, size, &nalUnits);
    for (const auto& nalu: nalUnits)
    {
        if (nalu.second < 4)
            break;
        uint32_t* ptr = (uint32_t*)(nalu.first - 4);
        *ptr = htonl(nalu.second);
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


void getSpsPps(const uint8_t* data, int32_t size,
    std::vector<std::vector<uint8_t>>& spsVector,
    std::vector<std::vector<uint8_t>>& ppsVector)
{
    std::vector<std::pair<const quint8*, size_t>> nalUnits;
    readNALUsFromAnnexBStream(data, size, &nalUnits);
    for (const auto& nalu: nalUnits)
    {
        if ((*nalu.first & 0x1f) == nuSPS)
        {
            if (nalu.second < 4)
            {
                NX_WARNING(NX_SCOPE_TAG, "Invalid sps found");
                continue;
            }
            spsVector.emplace_back(nalu.first, nalu.first + nalu.second);
        }
        if ((*nalu.first & 0x1f) == nuPPS)
        {
            if (nalu.second < 4)
            {
                NX_WARNING(NX_SCOPE_TAG, "Invalid pps found");
                continue;
            }
            ppsVector.emplace_back(nalu.first, nalu.first + nalu.second);
        }
    }
}

std::vector<uint8_t> buildExtraData(const uint8_t* data, int32_t size)
{
    std::vector<std::vector<uint8_t>> spsVector;
    std::vector<std::vector<uint8_t>> ppsVector;
    getSpsPps(data, size, spsVector, ppsVector);

    if (spsVector.empty() || ppsVector.empty())
    {
        NX_WARNING(NX_SCOPE_TAG, "Failed to write extra data, no sps/pps found");
        return std::vector<uint8_t>();
    }

    // Get extra data size
    int extradataSize = 5; // version, profile, profile compat, level, nal size length
    extradataSize += 1; // sps count
    for(const auto& sps: spsVector)
    {
        extradataSize += sizeof(uint16_t); // sps size
        extradataSize += sps.size(); // sps data
    }
    extradataSize += 1; // pps count
    for(const auto& pps: ppsVector)
    {
        extradataSize += sizeof(uint16_t); // pps size
        extradataSize += pps.size(); // pps data
    }

    auto sps = spsVector[0];
    auto pps = ppsVector[0];
    std::vector<uint8_t> extradata(extradataSize);
    BitStreamWriter bitstream(extradata.data(), extradata.data() + extradataSize);
    try
    {
        bitstream.putBits(8, 1); // version
        bitstream.putBits(8, sps[1]); // profile
        bitstream.putBits(8, sps[2]); // profile compat
        bitstream.putBits(8, sps[3]); // profile level
        bitstream.putBits(8, 0xff); // 6 bits reserved (111111) + 2 bits nal size length - 1: 3 (11)

        uint8_t numSps = uint8_t(spsVector.size()) | 0xe0;
        bitstream.putBits(8, numSps); // 3 bits reserved (111) + 5 bits number of sps

        for(const auto& spsData: spsVector)
        {
            bitstream.putBits(16, spsData.size());
            bitstream.putBytes(spsData.data(), spsData.size());
        }
        bitstream.putBits(8, ppsVector.size()); // number of pps
        for(const auto& ppsData: ppsVector)
        {
            bitstream.putBits(16, ppsData.size());
            bitstream.putBytes(ppsData.data(), ppsData.size());
        }
        bitstream.flushBits();
    }
    catch(const BitStreamException&)
    {
        NX_ERROR(NX_SCOPE_TAG, "Failed to write extra data");
        return std::vector<uint8_t>();
    }
    return extradata;
}


} // namespace nx::media_utils
