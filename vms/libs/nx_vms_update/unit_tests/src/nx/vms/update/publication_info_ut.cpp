// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/reflect/json.h>
#include <nx/vms/update/publication_info.h>

namespace nx::vms::update::test {

namespace os {

const nx::utils::OsInfo ubuntu{"linux_x64", "ubuntu"};
const nx::utils::OsInfo ubuntu14{"linux_x64", "ubuntu", "14.04"};
const nx::utils::OsInfo ubuntu16{"linux_x64", "ubuntu", "16.04"};
const nx::utils::OsInfo ubuntu18{"linux_x64", "ubuntu", "18.04"};

} // namespace os

TEST(PublicationInfoTest, bestVariantVersionSelection)
{
    const auto [info, _] = nx::reflect::json::deserialize<PublicationInfo>(
        R"({
            "packages": [
                {
                    "component": "server",
                    "platform": "linux_x64",
                    "variants": [{ "name": "ubuntu", "minimumVersion": "14.04" }],
                    "file": "ubuntu_14.04.zip"
                },
                {
                    "component": "server",
                    "platform": "linux_x64",
                    "variants": [{ "name": "ubuntu", "minimumVersion": "16.04" }],
                    "file": "ubuntu_16.04.zip"
                },
                {
                    "component": "server",
                    "platform": "linux_x64",
                    "variants": [{ "name": "ubuntu", "minimumVersion": "18.04" }],
                    "file": "ubuntu_18.04.zip"
                }
            ]
        })");

    const auto result = info.findPackage(update::Component::server, os::ubuntu16);
    EXPECT_TRUE(std::holds_alternative<update::Package>(result));
    EXPECT_EQ(std::get<update::Package>(result).file, "ubuntu_16.04.zip");
}

TEST(PublicationInfoTest, variantVersionBoundaries)
{
    const auto [info, _] = nx::reflect::json::deserialize<PublicationInfo>(
        R"({
            "packages": [
                {
                    "component": "server",
                    "platform": "linux_x64",
                    "variants": [
                        {
                            "name": "ubuntu",
                            "minimumVersion": "14.04",
                            "maximumVersion": "16.04"
                        }],
                    "file": "ubuntu.zip"
                }
            ]
        })");

    const auto result = info.findPackage(update::Component::server, os::ubuntu18);
    EXPECT_TRUE(std::holds_alternative<update::FindPackageError>(result));
    EXPECT_EQ(
        std::get<update::FindPackageError>(result),
        update::FindPackageError::osVersionNotSupported);
}

TEST(PublicationInfoTest, emptyOsVersionAndVersionedPackage)
{
    const auto [info, _] = nx::reflect::json::deserialize<PublicationInfo>(
        R"({
            "packages": [
                {
                    "component": "server",
                    "platform": "linux_x64",
                    "variants": [{ "name": "ubuntu", "minimumVersion": "14.04" }],
                    "file": "ubuntu.zip"
                }
            ]
        })");

    const auto result = info.findPackage(update::Component::server, os::ubuntu);
    EXPECT_TRUE(std::holds_alternative<update::FindPackageError>(result));
    EXPECT_EQ(
        std::get<update::FindPackageError>(result),
        update::FindPackageError::osVersionNotSupported);
}

TEST(PublicationInfoTest, preferPackageForCertainVariant)
{
    const auto [info, _] = nx::reflect::json::deserialize<PublicationInfo>(
        R"({
            "packages": [
                {
                    "component": "server",
                    "platform": "linux_x64",
                    "file": "universal.zip"
                },
                {
                    "component": "server",
                    "platform": "linux_x64",
                    "variants": [{ "name": "ubuntu" }],
                    "file": "ubuntu.zip"
                }
            ]
        })");

    const auto result = info.findPackage(update::Component::server, os::ubuntu);
    EXPECT_TRUE(std::holds_alternative<update::Package>(result));
    EXPECT_EQ(std::get<update::Package>(result).file, "ubuntu.zip");
}

TEST(PublicationInfoTest, preferVersionedVariant)
{
    const auto [info, _] = nx::reflect::json::deserialize<PublicationInfo>(
        R"({
            "packages": [
                {
                    "component": "server",
                    "platform": "linux_x64",
                    "variants": [{ "name": "ubuntu" }],
                    "file": "ubuntu.zip"
                },
                {
                    "component": "server",
                    "platform": "linux_x64",
                    "variants": [{ "name": "ubuntu", "minimumVersion": "16.04" }],
                    "file": "ubuntu_16.04.zip"
                }
            ]
        })");

    const auto result = info.findPackage(update::Component::server, os::ubuntu16);
    EXPECT_TRUE(std::holds_alternative<update::Package>(result));
    EXPECT_EQ(std::get<update::Package>(result).file, "ubuntu_16.04.zip");
}

TEST(PublicationInfoTest, aSyntheticTest)
{
    const auto [info, _] = nx::reflect::json::deserialize<PublicationInfo>(
        R"({
            "packages": [
                {
                    "component": "server",
                    "platform": "linux_x64",
                    "file": "universal.zip"
                },
                {
                    "component": "server",
                    "platform": "linux_x64",
                    "variants": [{ "name": "ubuntu" }],
                    "file": "ubuntu.zip"
                },
                {
                    "component": "server",
                    "platform": "linux_x64",
                    "variants": [{ "name": "ubuntu", "minimumVersion": "18.04" }],
                    "file": "ubuntu_18.04.zip"
                }
            ]
        })");

    const auto result = info.findPackage(update::Component::server, os::ubuntu14);
    EXPECT_TRUE(std::holds_alternative<update::Package>(result));
    EXPECT_EQ(std::get<update::Package>(result).file, "ubuntu.zip");
}

} // namespace nx::vms::update::test
