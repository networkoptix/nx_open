// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <string>

#include <gtest/gtest.h>

#include <nx/reflect/json.h>
#include <nx/vms/api/data/site_health_message.h>

namespace nx::vms::api::test {

namespace {

using namespace std::chrono;

const nx::Uuid kIntercomId("a92a5ef6-d5e4-79bb-3778-7e70a051b123");

SiteHealthMessage makeIntercomMessage(SiteHealthMessageType type, bool active)
{
    return SiteHealthMessage{
        .type = type,
        .active = active,
        .timestampUs = microseconds(1791196156735000),
        .resourceIds = {kIntercomId},
    };
}

/** The server-to-client path: reflect-JSON text, read into a default-constructed message. */
SiteHealthMessage sendAsJson(const SiteHealthMessage& message)
{
    const std::string json = nx::reflect::json::serialize(message);
    SiteHealthMessage received;
    const auto result = nx::reflect::json::deserialize(json, &received);
    EXPECT_TRUE(result.success) << result.errorDescription << "; json: " << json;
    return received;
}

void expectSameContent(const SiteHealthMessage& expected, const SiteHealthMessage& actual)
{
    EXPECT_EQ(expected.type, actual.type);
    EXPECT_EQ(expected.timestampUs, actual.timestampUs);
    EXPECT_EQ(expected.resourceIds, actual.resourceIds);
    EXPECT_EQ(expected.attributes, actual.attributes);
}

} // namespace

/**
 * The server hides the "Calling..." informer of an intercom by sending showIntercomInformer with
 * active=false. A client must receive it as inactive.
 */
TEST(SiteHealthMessageJson, hiddenIntercomInformerStaysInactive)
{
    const auto sent =
        makeIntercomMessage(SiteHealthMessageType::showIntercomInformer, /*active*/ false);

    const auto received = sendAsJson(sent);

    EXPECT_FALSE(received.active) << "Sent as: " << nx::reflect::json::serialize(sent);
    expectSameContent(sent, received);
}

} // namespace nx::vms::api::test
