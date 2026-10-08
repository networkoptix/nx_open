// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <string>
#include <utility>

#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/reflect/json.h>
#include <nx/utils/log/to_string.h>
#include <nx/vms/api/data/device_ptz_model.h>

namespace nx::vms::api::test {

namespace {

constexpr PtzApiType kApiTypes[]{
    PtzApiType::operational,
    PtzApiType::configurational,
    PtzApiType::any,
};

} // namespace

TEST(DevicePtzModel, ApiTypesKeepWireValues)
{
    EXPECT_EQ(std::to_underlying(PtzApiType::operational), 1);
    EXPECT_EQ(std::to_underlying(PtzApiType::configurational), 2);
    EXPECT_EQ(std::to_underlying(PtzApiType::any), 3);

    EXPECT_EQ(nx::toString(PtzApiType::operational), "operational");
    EXPECT_EQ(nx::toString(PtzApiType::configurational), "configurational");
    EXPECT_EQ(nx::toString(PtzApiType::any), "any");

    for (const auto type: kApiTypes)
    {
        const auto name = nx::toString(type).toStdString();
        SCOPED_TRACE(name);
        const auto quotedName = '"' + name + '"';
        EXPECT_EQ(nx::reflect::json::serialize(type), quotedName);
        EXPECT_EQ(QJson::serialized(type).toStdString(), quotedName);
    }
}

TEST(DevicePtzModel, ApiTypesDeserializeExistingValues)
{
    for (const auto type: kApiTypes)
    {
        const auto name = nx::toString(type).toStdString();
        const auto number = std::to_string(std::to_underlying(type));
        for (const auto& input: {'"' + name + '"', '"' + number + '"', number})
        {
            SCOPED_TRACE(input);
            PtzApiType parsed;
            ASSERT_TRUE(nx::reflect::json::deserialize(input, &parsed));
            EXPECT_EQ(parsed, type);
            ASSERT_TRUE(QJson::deserialize(QByteArray::fromStdString(input), &parsed));
            EXPECT_EQ(parsed, type);
        }
    }
}

TEST(DevicePtzModel, HiddenApiTypeRemainsAvailableInCode)
{
    EXPECT_EQ(std::to_underlying(PtzApiType::none), 0);
    for (const auto input: {R"("none")", R"("0")", "0"})
    {
        SCOPED_TRACE(input);
        PtzApiType parsed = PtzApiType::operational;
        ASSERT_TRUE(nx::reflect::json::deserialize(input, &parsed));
        EXPECT_EQ(parsed, PtzApiType::none);
        parsed = PtzApiType::operational;
        ASSERT_TRUE(QJson::deserialize(QByteArray(input), &parsed));
        EXPECT_EQ(parsed, PtzApiType::none);
    }
}

template<typename T>
class DevicePtzModelDefaults: public ::testing::Test
{
};

using PtzRequestModels = ::testing::
    Types<PtzPositionFilter, PtzPosition, PtzMovement, PtzViewportMove, PtzAuxiliaryCommand>;
TYPED_TEST_SUITE(DevicePtzModelDefaults, PtzRequestModels);

TYPED_TEST(DevicePtzModelDefaults, OmittedApiUsesOperational)
{
    TypeParam data;
    EXPECT_EQ(data.api, PtzApiType::operational);
    ASSERT_TRUE(nx::reflect::json::deserialize("{}", &data));
    EXPECT_EQ(data.api, PtzApiType::operational);
    ASSERT_TRUE(QJson::deserialize(QByteArray("{}"), &data));
    EXPECT_EQ(data.api, PtzApiType::operational);
}

} // namespace nx::vms::api::test
