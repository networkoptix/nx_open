// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/rtp/rtp.h>

TEST(RtpHeader, Read)
{
    {
        nx::rtp::RtpHeaderData header;
        ASSERT_FALSE(header.read(nullptr, 0));
    }

    {
        uint8_t data[] = {0x80, 0xc8, 0x00, 0x06, 0x79, 0xcd, 0x66, 0x0c, 0xbc, 0x26, 0xaf};
        nx::rtp::RtpHeaderData header;
        ASSERT_FALSE(header.read(data, sizeof(data)));
    }

    {
        uint8_t data[] = {0x80, 0x60, 0x00, 0x00, 0x00, 0x00, 0x03, 0x84, 0x00, 0x00, 0x02, 0x2b};
        nx::rtp::RtpHeaderData header;
        ASSERT_TRUE(header.read(data, sizeof(data)));
        ASSERT_EQ(header.payloadOffset, 12);
        ASSERT_EQ(header.payloadSize, 0);
        ASSERT_FALSE(header.extension.has_value());
    }

    // Padding bigger than overall size.
    {
        uint8_t data[] = {0xa0, 0x60, 0x00, 0x00, 0x00, 0x00, 0x03, 0x84, 0x00, 0x00, 0x02, 0x2b,
            0x00, 0x00, 0x00, //< Payload.
            0x10 //< Padding size.
        };
        nx::rtp::RtpHeaderData header;
        ASSERT_FALSE(header.read(data, sizeof(data)));
    }

    // Padding bigger than payload size.
    {
        uint8_t data[] = {0xa0, 0x60, 0x00, 0x00, 0x00, 0x00, 0x03, 0x84, 0x00, 0x00, 0x02, 0x2b,
            0x00, 0x00, 0x00, //< Payload.
            0x05 //< Padding size.
        };
        nx::rtp::RtpHeaderData header;
        ASSERT_FALSE(header.read(data, sizeof(data)));
    }

    // Normal padding
    {
        uint8_t data[] = {0xa0, 0x60, 0x00, 0x00, 0x00, 0x00, 0x03, 0x84, 0x00, 0x00, 0x02, 0x2b,
            0x00, 0x00, 0x00, //< Payload.
            0x02 //< Padding size.
        };
        nx::rtp::RtpHeaderData header;
        ASSERT_TRUE(header.read(data, sizeof(data)));
        ASSERT_EQ(header.payloadOffset, 12);
        ASSERT_EQ(header.payloadSize, 2);
        ASSERT_FALSE(header.extension.has_value());
    }
}
