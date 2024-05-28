// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <utils/media/hevc_common.h>
#include <utils/media/hevc/sequence_parameter_set.h>

using namespace nx::media::hevc;

TEST(H265, SpsParse)
{
    uint8_t data[] = {
        0x42, 0x01, 0x01, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
        0x00, 0x03, 0x00, 0x96, 0xa0, 0x01, 0x40, 0x20, 0x05, 0xa1, 0xfe, 0x5a, 0xee, 0xf3, 0x77,
        0x72, 0x5d, 0x60, 0x2d, 0xc0, 0x40, 0x40, 0x41, 0x00, 0x00, 0x03, 0x00, 0x01, 0x00, 0x00,
        0x03, 0x00, 0x1e, 0x41, 0x8f, 0xa0 };

    SequenceParameterSet sps;
    ASSERT_TRUE(parseNalUnit(data, sizeof(data), sps));
}

TEST(H265, SpsParse1)
{
    uint8_t data[] = {
        0x42, 0x01, 0x01, 0x00, 0x80, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
        0x00, 0x03, 0x00, 0x00, 0xa0, 0x02, 0x80, 0x80, 0x2d, 0x1f, 0xe5, 0x91, 0x64, 0x91, 0xb4,
        0x12, 0xe4, 0x92, 0x6b, 0x25, 0xcd, 0x29, 0x2a, 0x69, 0xc9, 0xca, 0x4c, 0xff, 0xf9, 0xd9};

    SequenceParameterSet sps;
    ASSERT_TRUE(parseNalUnit(data, sizeof(data), sps));
    ASSERT_EQ(sps.sps_sub_layer_ordering_info_vector.size(), 1);
    ASSERT_EQ(sps.sps_sub_layer_ordering_info_vector[0].sps_max_dec_pic_buffering_minus1, 3);
}
