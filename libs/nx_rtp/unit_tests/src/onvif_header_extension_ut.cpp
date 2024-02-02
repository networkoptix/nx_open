// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/rtp/onvif_header_extension.h>

TEST(OnvifHeaderExtension, readWrite)
{
    using namespace nx::rtp;
    uint8_t buffer[OnvifHeaderExtension::kSize];
    OnvifHeaderExtension header;
    std::chrono::microseconds time(123456789000);
    header.ntp = time;
    ASSERT_EQ(header.write(buffer, OnvifHeaderExtension::kSize), OnvifHeaderExtension::kSize);
    OnvifHeaderExtension headerLoaded;
    ASSERT_TRUE(headerLoaded.read(buffer, OnvifHeaderExtension::kSize));
    ASSERT_EQ(headerLoaded.ntp, time);

    ASSERT_FALSE(headerLoaded.read(buffer, 4));
    ASSERT_FALSE(headerLoaded.read(buffer, 0));
    ASSERT_FALSE(headerLoaded.read(nullptr, 0));
}
