// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/streaming/rtsp_restreamer.h>

bool checkRegex(const std::string& hostname, const std::string& expectedUid)
{
    std::smatch match;
    try
    {
        std::regex cloudAddressRegex(
            nx::streaming::RtspRestreamer::cloudAddressTemplate(),
            std::regex_constants::ECMAScript | std::regex_constants::icase);
        bool matched = std::regex_match(
            hostname,
            match,
            cloudAddressRegex);
        if (!matched)
        {
            return false;
        }
    }
    catch (const std::regex_error& error)
    {
        std::string empty;
        EXPECT_EQ(error.what(), empty);
        return false;
    }
    EXPECT_EQ(match.size(), 4);
    EXPECT_EQ(match[2], expectedUid);
    return true;
}

TEST(RtspRestreamer, ValidateRegex1)
{
    EXPECT_TRUE(checkRegex(
        "5698dc51-2755-48c8-b7d6-b22a5a5b9123.relay.relay.cloud.hdw.mx",
        "5698dc51-2755-48c8-b7d6-b22a5a5b9123"));
}

TEST(RtspRestreamer, ValidateRegex2)
{
    // Already in cloud format.
    EXPECT_FALSE(checkRegex(
        "5698dc51-2755-48c8-b7d6-b22a5a5b9123",
        "5698dc51-2755-48c8-b7d6-b22a5a5b9123"));
}

TEST(RtspRestreamer, ValidateRegex3)
{
    EXPECT_TRUE(checkRegex(
        "5698dc51-2755-48c8-b7d6-b22a5a5b9122."
        "5698dc51-2755-48c8-b7d6-b22a5a5b9123.relay.relay.cloud.hdw.mx",
        "5698dc51-2755-48c8-b7d6-b22a5a5b9123"));
}

TEST(RtspRestreamer, ValidateRegex4)
{
    EXPECT_TRUE(checkRegex(
        "5698dc51-2755-48c8-b7d6-b22a5a5b9120."
        "5698dc51-2755-48c8-b7d6-b22a5a5b9121."
        "5698dc51-2755-48c8-b7d6-b22a5a5b9122."
        "5698dc51-2755-48c8-b7d6-b22a5a5b9123.relay.relay.cloud.hdw.mx",
        "5698dc51-2755-48c8-b7d6-b22a5a5b9123"));
}

TEST(RtspRestreamer, ValidateRegex5)
{
    EXPECT_TRUE(checkRegex(
        "blablabla."
        "5698dc51-2755-48c8-b7d6-b22a5a5b9123.relay.relay.cloud.hdw.mx",
        "5698dc51-2755-48c8-b7d6-b22a5a5b9123"));
}

TEST(RtspRestreamer, ValidateRegex6)
{
    EXPECT_FALSE(checkRegex(
        "15698dc51-2755-48c8-b7d6-b22a5a5b9123.relay.relay.cloud.hdw.mx",
        "5698dc51-2755-48c8-b7d6-b22a5a5b9123"));
}

TEST(RtspRestreamer, ValidateRegex7)
{
    EXPECT_FALSE(checkRegex(
        "www.example.com",
        "5698dc51-2755-48c8-b7d6-b22a5a5b9123"));
}
