// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <media/filters/remove_aud_delimiter.h>
#include <nx/media/video_data_packet.h>

TEST(mediaFilters, H264RemoveAudForAnexB)
{
    // Check crash on invalid data.
    H2645RemoveAudDelimiter filter;
    QnWritableCompressedVideoDataPtr result(new QnWritableCompressedVideoData());
    QByteArray data = QByteArray::fromHex(
        // 00 00 00 01 67 ...   (SPS, nal_unit_type = 7)
        "00000001"
        "6742001E95A8280F2E02D080"
        // 00 00 00 01 68 ...   (PPS, nal_unit_type = 8)
        "00000001"
        "68CE06E2"
        // 00 00 00 01 09 F0    (AUD, nal_unit_type = 9)
        "00000001"
        "09F0"
        // 00 00 00 01 65 ...   (IDR, nal_unit_type = 5) - short payload
        "00000001"
        "658884000A7B20"
    );
    result->m_data.write(data);
    result->compressionType = AV_CODEC_ID_H264;
    auto updated = std::dynamic_pointer_cast<const QnWritableCompressedVideoData>(
        filter.processData(result));
    ASSERT_EQ(data.size() - 6, updated->m_data.size());
}

TEST(mediaFilters, H265RemoveAudForAnexB)
{
    // Check crash on invalid data.
    H2645RemoveAudDelimiter filter;
    QnWritableCompressedVideoDataPtr result(new QnWritableCompressedVideoData());
    // AnnexB: 00 00 00 01 + HEVC NAL (2-byte header + payload)
    // VPS (type 32), SPS (33), PPS (34), AUD (35), IDR (19) - short payload
    QByteArray data = QByteArray::fromHex(
        // VPS (nal_unit_type = 32)
        "00000001"
        "4001"        // 2-byte NAL header
        "0C01FFFF016000000300900000030000030078A002"  // tiny-ish dummy payload

        // SPS (nal_unit_type = 33)
        "00000001"
        "4201"
        "01016000000300900000030000030078A003C08010E5"

        // PPS (nal_unit_type = 34)
        "00000001"
        "4401"
        "C0F1832A"

        // AUD (nal_unit_type = 35)
        "00000001"
        "4601"
        "50"          // minimal dummy payload

        // IDR_W_RADL (nal_unit_type = 19) - short
        "00000001"
        "2601"
        "9A22"      // short dummy payload
    );
    result->m_data.write(data);
    result->compressionType = AV_CODEC_ID_H265;
    auto updated = std::dynamic_pointer_cast<const QnWritableCompressedVideoData>(
        filter.processData(result));
    ASSERT_EQ(data.size() - 7, updated->m_data.size());
}

TEST(mediaFilters, H264RemoveAudForMp4)
{
    // Check crash on invalid data.
    H2645RemoveAudDelimiter filter;
    QnWritableCompressedVideoDataPtr result(new QnWritableCompressedVideoData());
    QByteArray data = QByteArray::fromHex(
        // SPS length = 12 (0x0000000C)
        "0000000C"
        "6742001E95A8280F2E02D080"

        // PPS length = 4
        "00000004"
        "68CE06E2"

        // AUD length = 2
        "00000002"
        "09F0"

        // IDR length = 7
        "00000007"
        "658884000A7B20"
    );
    result->m_data.write(data);
    result->compressionType = AV_CODEC_ID_H264;

    // FFmpeg extradata for MP4/H.264 (avcC), no box header, just avcC payload
    QByteArray extraDataH264AvcC = QByteArray::fromHex(
        "01"        // configurationVersion
        "42"        // AVCProfileIndication (from SPS: 0x42)
        "00"        // profile_compatibility
        "1E"        // AVCLevelIndication (0x1E)
        "FF"        // 111111 + lengthSizeMinusOne(3) => 4-byte NAL lengths
        "E1"        // 111 + numOfSPS(1)
        "000C"      // SPS length (12)
        "6742001E95A8280F2E02D080"  // SPS
        "01"        // numOfPPS(1)
        "0004"      // PPS length (4)
        "68CE06E2"  // PPS
    );
    auto context = std::make_shared<CodecParameters>();
    context->setExtradata((const uint8_t*) extraDataH264AvcC.data(), extraDataH264AvcC.size());
    result->context = context;

    auto updated = std::dynamic_pointer_cast<const QnWritableCompressedVideoData>(
        filter.processData(result));
    ASSERT_EQ(data.size() - 6, updated->m_data.size());
}

TEST(mediaFilters, H265RemoveAudForMp4)
{
    // Check crash on invalid data.
    H2645RemoveAudDelimiter filter;
    QnWritableCompressedVideoDataPtr result(new QnWritableCompressedVideoData());
    // AnnexB: 00 00 00 01 + HEVC NAL (2-byte header + payload)
    // VPS (type 32), SPS (33), PPS (34), AUD (35), IDR (19) - short payload
    QByteArray data = QByteArray::fromHex(
        // VPS length = 23 (0x00000017)
        "00000017"
        "40010C01FFFF016000000300900000030000030078A002"

        // SPS length = 24 (0x00000018)
        "00000018"
        "420101016000000300900000030000030078A003C08010E5"

        // PPS length = 6
        "00000006"
        "4401C0F1832A"

        // AUD length = 3
        "00000003"
        "460150"

        // IDR length = 5 (IDR_W_RADL nal_unit_type=19)
        "00000004"
        "26019A22"
    );
    result->m_data.write(data);
    result->compressionType = AV_CODEC_ID_H265;

    // FFmpeg extradata for MP4/H.265 (hvcC), no box header, just hvcC payload
    QByteArray extraDataH265HvcC = QByteArray::fromHex(
        "01"                // configurationVersion

        "01"                // general_profile_space(0), general_tier_flag(0), general_profile_idc(1)
        "00000000"          // general_profile_compatibility_flags (4 bytes)
        "000000000000"      // general_constraint_indicator_flags (6 bytes)
        "78"                // general_level_idc (0x78 = 120, reasonable dummy)

        "F000"              // min_spatial_segmentation_idc (reserved '1111' + 0)
        "FC"                // parallelismType (reserved '111111' + 0)
        "FD"                // chromaFormat (reserved '111111' + 1 => 4:2:0)
        "F8"                // bitDepthLumaMinus8 (reserved '11111' + 0 => 8-bit)
        "F8"                // bitDepthChromaMinus8 (reserved '11111' + 0 => 8-bit)

        "0000"              // avgFrameRate
        "0F"                // constantFrameRate=0, numTemporalLayers=1, temporalIdNested=1, lengthSizeMinusOne=3
        "03"                // numOfArrays = 3

        // Array 1: VPS (nal_unit_type = 32)
        "A0"                // array_completeness=1, reserved=0, nal_unit_type=32
        "0001"              // numNalus = 1
        "0017"              // nalUnitLength = 23
        "40010C01FFFF016000000300900000030000030078A002"

        // Array 2: SPS (nal_unit_type = 33)
        "A1"
        "0001"
        "0018"              // 24
        "420101016000000300900000030000030078A003C08010E5"

        // Array 3: PPS (nal_unit_type = 34)
        "A2"
        "0001"
        "0006"
        "4401C0F1832A"
    );
    auto context = std::make_shared<CodecParameters>();
    context->setExtradata((const uint8_t*)extraDataH265HvcC.data(), extraDataH265HvcC.size());
    result->context = context;

    auto updated = std::dynamic_pointer_cast<const QnWritableCompressedVideoData>(
        filter.processData(result));
    ASSERT_EQ(data.size() - 7, updated->m_data.size());
}
