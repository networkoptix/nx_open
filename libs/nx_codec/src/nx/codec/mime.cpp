// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mime.h"

#include <cstdint>
#include <vector>

#include <nx/codec/hevc/hevc_decoder_configuration_record.h>
#include <nx/utils/log/log.h>

#include "nal_extradata.h"
#include "nal_units.h"

namespace nx::media {

namespace {

template <int Size>
std::string dumpBytesOmitted(const void* ptr, bool withDots)
{
    uint8_t data[Size];
    memcpy(data, ptr, Size);
    std::string result;
    char buf[3];
    for (int i = 0; i < Size; ++i)
    {
        if (data[i] != 0)
        {
            snprintf(buf, sizeof(buf), "%X", data[i]);
            if (withDots && !result.empty())
                result += ".";
            result += buf;
        }
    }
    if (result.empty())
        result += "0";
    return result;
};

} // namespace

std::string getMimeType(const AVCodecParameters* codecpar)
{
    std::string result;
    if (!codecpar)
        return result;
    switch (codecpar->codec_id)
    {
        // Video codecs.
        // Support only H.264 and H.265. Note that MSE also supports VP8/VP9 packed into WebM container.
        case AV_CODEC_ID_H264:
        {
            std::vector<uint8_t> data = nx::media::nal::readH264SeqHeaderFromExtraData(
                codecpar->extradata, codecpar->extradata_size);
            if (data.empty())
            {
                // After transcoding, extradata is already in annex-b format.
                data.insert(
                    data.begin(),
                    codecpar->extradata,
                    codecpar->extradata + codecpar->extradata_size);
            }

            const uint8_t* dataStart = (const uint8_t*) &data[0];
            const uint8_t* dataEnd = dataStart + data.size();
            const auto nalu = NALUnit::findNextNAL(dataStart, dataEnd);
            if (nalu && dataEnd - nalu >= 4 && (nalu[0] & 0x1F) == NALUnitType::nuSPS)
            {
                constexpr size_t kAvc1BufferSize = 12;
                result.resize(kAvc1BufferSize);
                const auto len = snprintf(
                    result.data(), result.size(), "avc1.%02x%02x%02x", nalu[1], nalu[2], nalu[3]);
                NX_ASSERT(len == kAvc1BufferSize - 1); //< No trailing zero.
                result.resize(len);
            }
            else
            {
                NX_DEBUG(NX_SCOPE_TAG, "Can't parse H.264 extradata format");
                result = "avc1"; //< Is enough for some browsers.
            }
            break;
        }
        case AV_CODEC_ID_H265:
        {
            // Hope we will never transcode to h265, so use extradata only in hvcc format.
            nx::media::hevc::HEVCDecoderConfigurationRecord hvcc;
            if (hvcc.read(codecpar->extradata, codecpar->extradata_size))
            {
                // ISO/IEC 14496-15, Annex E.3.
                // Example: "hvc1.1.6.L150.B0"
                result += "hvc1.";
                if (hvcc.general_profile_space >= 1 && hvcc.general_profile_space <= 3)
                    result += 'A' + hvcc.general_profile_space - 1;
                result += std::to_string(hvcc.general_profile_idc);
                result += ".";
                result += dumpBytesOmitted<sizeof(hvcc.general_profile_compatibility_flags)>(
                    &hvcc.general_profile_compatibility_flags,
                    /*withDots*/ false);
                result += ".";
                result += (hvcc.general_tier_flag ? "H" : "L");
                result += std::to_string(hvcc.general_level_idc);
                result += ".";
                result += dumpBytesOmitted<6>( //< Actually 6 of 8 bytes.
                    &hvcc.general_constraint_indicator_flags,
                    /*withDots*/ true);
            }
            else
            {
                NX_DEBUG(NX_SCOPE_TAG, "Can't parse H.265 extradata format");
                result = "hvc1";
            }
            break;
        }
        // Audio codecs.
        case AV_CODEC_ID_MP2:
            return "mp4a.40.33";
        case AV_CODEC_ID_MP3:
            return "mp4a.40.34";
        case AV_CODEC_ID_AAC:
            // Should be mp4a.40.5 for aac_he and mp4a.40.2 for aac_lc.
            return "mp4a.40.2";
        case AV_CODEC_ID_AC3:
            return "ac-3";
        case AV_CODEC_ID_EAC3:
            return "ec-3";
        default:
            NX_ASSERT(false, "Extradata should not be filled for this codec: %1", codecpar->codec_id);
            break;
    }
    return result;
}

} // namespace nx::media
