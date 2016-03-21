#include "h264_utils.h"

#ifdef ENABLE_DATA_PROVIDERS

#include "core/resource/param.h"
#include "utils/media/nalUnits.h"


namespace
{
bool isH264SeqHeaderInExtraData(const QnConstCompressedVideoDataPtr data)
{
    return data->context &&
        data->context->ctx() &&
        data->context->ctx()->extradata_size >= 7 &&
        data->context->ctx()->extradata[0] == 1;
}

//dishonorably stolen from libavcodec source
#ifndef AV_RB16
#   define AV_RB16(x)                           \
    ((((const uint8_t*)(x))[0] << 8) |          \
      ((const uint8_t*)(x))[1])
#endif

void readH264NALUsFromExtraData(
    const QnConstCompressedVideoDataPtr data,
    std::vector<std::pair<const quint8*, size_t>>* const nalUnits)
{
    const unsigned char* p = data->context->ctx()->extradata;

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
        if (nalsize > data->context->ctx()->extradata_size - (p - data->context->ctx()->extradata))
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
        if (nalsize > data->context->ctx()->extradata_size - (p - data->context->ctx()->extradata))
            break;

        nalUnits->emplace_back((const quint8*)p, nalsize);
        p += nalsize;
    }
}

void readH264NALUsFromAnnexBStream(
    const QnConstCompressedVideoDataPtr data,
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
        Q_ASSERT(nextNalu > curNalu);
        //skipping leading_zero_8bits and trailing_zero_8bits
        while ((naluEnd > curNalu) && (*(naluEnd - 1) == 0))
            --naluEnd;
        if (naluEnd > curNalu)
            nalUnits->emplace_back((const quint8*)curNalu, naluEnd - curNalu);
    }
}
}

void extractSpsPps(
    const QnCompressedVideoDataPtr& videoData,
    QSize* const newResolution,
    std::map<QString, QString>* const customStreamParams)
{
    //vector<pair<nalu buf, nalu size>>
    std::vector<std::pair<const quint8*, size_t>> nalUnits;
    if (isH264SeqHeaderInExtraData(videoData))
        readH264NALUsFromExtraData(videoData, &nalUnits);
    else
        readH264NALUsFromAnnexBStream(videoData, &nalUnits);

    //generating profile-level-id and sprop-parameter-sets as in rfc6184
    QByteArray profileLevelID;
    QByteArray spropParameterSets;
    bool spsFound = false;
    bool ppsFound = false;

    for (const std::pair<const quint8*, size_t>& nalu : nalUnits)
    {
        switch (*nalu.first & 0x1f)
        {
            case nuSPS:
                if (nalu.second < 4)
                    continue;   //invalid sps

                if (spsFound)
                    continue;
                else
                    spsFound = true;

                if (newResolution)
                {
                    //parsing sps to get resolution
                    SPSUnit sps;
                    sps.decodeBuffer(nalu.first, nalu.first + nalu.second);
                    sps.deserialize();
                    newResolution->setWidth(sps.getWidth());
                    newResolution->setHeight(sps.pic_height_in_map_units * 16);

                    //reading frame cropping settings
                    const unsigned int subHeightC = sps.chroma_format_idc == 1 ? 2 : 1;
                    const unsigned int cropUnitY = (sps.chroma_format_idc == 0)
                        ? (2 - sps.frame_mbs_only_flag)
                        : (subHeightC * (2 - sps.frame_mbs_only_flag));
                    const unsigned int originalFrameCropTop = cropUnitY * sps.frame_crop_top_offset;
                    const unsigned int originalFrameCropBottom = cropUnitY * sps.frame_crop_bottom_offset;
                    newResolution->setHeight(newResolution->height() - (originalFrameCropTop + originalFrameCropBottom));
                }

                if (customStreamParams)
                {
                    profileLevelID = QByteArray::fromRawData((const char*)nalu.first + 1, 3).toHex();
                    spropParameterSets = NALUnit::decodeNAL(
                        QByteArray::fromRawData((const char*)nalu.first, static_cast<int>(nalu.second))).toBase64() +
                        "," + spropParameterSets;
                }
                break;

            case nuPPS:
                if (ppsFound)
                    continue;
                else
                    ppsFound = true;

                if (customStreamParams)
                {
                    spropParameterSets += NALUnit::decodeNAL(
                        QByteArray::fromRawData((const char*)nalu.first, static_cast<int>(nalu.second))).toBase64();
                }
                break;
        }
    }

    if (customStreamParams)
    {
        if (!profileLevelID.isEmpty())
            customStreamParams->emplace(Qn::PROFILE_LEVEL_ID_PARAM_NAME, QLatin1String(profileLevelID));
        if (!spropParameterSets.isEmpty())
            customStreamParams->emplace(Qn::SPROP_PARAMETER_SETS_PARAM_NAME, QLatin1String(spropParameterSets));
    }
}

#endif
