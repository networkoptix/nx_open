// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/update/releases_info.h>

namespace nx::vms::update::test {

namespace {

// In all the following constants values of `release_date` and `release_delivery_days` do not
// matter. Date filtering is not done in the `ReleasesInfo` structure. The only thing it checks are
// they differ from 0.

const ReleaseInfo vmsRelease5_0{
    .version{5, 0, 0, 100},
    .product = Product::vms,
    .protocol_version = 50100,
    .release_date = 1,
    .release_delivery_days = 1,
};

const ReleaseInfo vmsRelease5_1{
    .version{5, 1, 0, 200},
    .product = Product::vms,
    .protocol_version = 51200,
    .release_date = 1,
    .release_delivery_days = 1,
};

const ReleaseInfo clientRelease5_0{
    .version{5, 0, 0, 150},
    .product = Product::desktop_client,
    .protocol_version = 50100,
    .release_date = 1,
    .release_delivery_days = 1,
};

std::optional<ReleaseInfo> selectVmsRelease(
    QVector<ReleaseInfo> releases,
    const nx::utils::SoftwareVersion& currentVersion)
{
    return ReleasesInfo{.releases{releases}}.selectVmsRelease(currentVersion);
}

std::optional<ReleaseInfo> selectDesktopClientRelease(
    QVector<ReleaseInfo> releases,
    const nx::utils::SoftwareVersion& currentVersion,
    const PublicationType publicationType,
    const int protocolVersion)
{
    return ReleasesInfo{.releases{releases}}.selectDesktopClientRelease(
        currentVersion, publicationType, protocolVersion);
}

} // namespace

TEST(ReleasesInfoTest, simpleVmsReleaseSelection)
{
    auto release = selectVmsRelease({vmsRelease5_0}, {4, 2});
    ASSERT_TRUE(release.has_value());
    EXPECT_EQ(release->version, vmsRelease5_0.version);
}

TEST(ReleasesInfoTest, newerReleaseIsSelected)
{
    auto release = selectVmsRelease({vmsRelease5_0, vmsRelease5_1}, {4, 2});
    EXPECT_EQ(release->version, vmsRelease5_1.version);
}

TEST(ReleasesInfoTest, newerBuildIsSelected)
{
    auto vmsRelease5_0_0_101 = vmsRelease5_0;
    vmsRelease5_0_0_101.version.build = 101;

    auto release = selectVmsRelease({vmsRelease5_0, vmsRelease5_0_0_101}, {4, 2});
    EXPECT_EQ(release->version, vmsRelease5_0_0_101.version);
}

TEST(ReleasesInfoTest, olderVersionIsNotSelected)
{
    auto release = selectVmsRelease({vmsRelease5_0}, {5, 1});
    EXPECT_FALSE(release.has_value());
}

TEST(ReleasesInfoTest, clientReleaseIsNotSelectedForVms)
{
    auto release = selectVmsRelease({vmsRelease5_0, clientRelease5_0}, {5, 0});
    EXPECT_EQ(release->version, vmsRelease5_0.version);
}

TEST(ReleasesInfoTest, onlyReleasesAreSelectedForVms)
{
    auto vmsRelease = vmsRelease5_0;
    vmsRelease.publication_type = PublicationType::rc;
    auto release = selectVmsRelease({vmsRelease}, {4, 2});
    EXPECT_FALSE(release.has_value());
}

TEST(ReleasesInfoTest, onlyPublishedBuildsAreSelectedForVms)
{
    auto vmsRelease = vmsRelease5_0;
    vmsRelease.release_date = 0;
    vmsRelease.release_delivery_days = 0;
    auto release = selectVmsRelease({vmsRelease}, {4, 2});
    EXPECT_FALSE(release.has_value());
}

TEST(ReleasesInfoTest, protocolVersionMustBeSameForClient)
{
    auto release = selectDesktopClientRelease(
        {clientRelease5_0},
        {5, 0}, PublicationType::release, clientRelease5_0.protocol_version - 1);
    EXPECT_FALSE(release.has_value());

    release = selectDesktopClientRelease(
        {clientRelease5_0},
        {5, 0}, PublicationType::release, clientRelease5_0.protocol_version + 1);
    EXPECT_FALSE(release.has_value());
}

TEST(ReleasesInfoTest, vmsReleaseCanBeSelectedForClient)
{
    auto release = selectDesktopClientRelease(
        {vmsRelease5_0}, {5, 0}, PublicationType::release, vmsRelease5_0.protocol_version);
    EXPECT_EQ(release->version, vmsRelease5_0.version);
}

TEST(ReleasesInfoTest, thereIsNoPreferenceForClientReleases)
{
    auto vmsRelease = vmsRelease5_0;
    vmsRelease.version.build = clientRelease5_0.version.build + 1;

    auto release = selectDesktopClientRelease(
        {clientRelease5_0, vmsRelease},
        {5, 0}, PublicationType::release, vmsRelease5_0.protocol_version);
    EXPECT_EQ(release->version, vmsRelease.version);
}

TEST(ReleasesInfoTest, clientSelectsMoreReleasishBuild)
{
    auto release = selectDesktopClientRelease(
        {clientRelease5_0},
        {5, 0}, PublicationType::beta, clientRelease5_0.protocol_version);
    EXPECT_EQ(release->version, clientRelease5_0.version);

    release = selectDesktopClientRelease(
        {clientRelease5_0},
        {5, 0}, PublicationType::rc, clientRelease5_0.protocol_version);
    EXPECT_EQ(release->version, clientRelease5_0.version);

    auto rcRelease = clientRelease5_0;
    rcRelease.publication_type = PublicationType::rc;

    release = selectDesktopClientRelease(
        {rcRelease},
        {5, 0}, PublicationType::beta, rcRelease.protocol_version);
    EXPECT_EQ(release->version, rcRelease.version);
}

TEST(ReleasesInfoTest, clientDoesNotSelectsLessReleasishBuild)
{
    auto clientRelease = clientRelease5_0;
    clientRelease.publication_type = PublicationType::rc;

    auto release = selectDesktopClientRelease(
        {clientRelease},
        {5, 0}, PublicationType::release, clientRelease.protocol_version);
    EXPECT_FALSE(release.has_value());

    clientRelease.publication_type = PublicationType::beta;

    release = selectDesktopClientRelease(
        {clientRelease},
        {5, 0}, PublicationType::release, clientRelease.protocol_version);
    EXPECT_FALSE(release.has_value());

    release = selectDesktopClientRelease(
        {clientRelease},
        {5, 0}, PublicationType::rc, clientRelease.protocol_version);
    EXPECT_FALSE(release.has_value());
}

} // namespace nx::vms::update::test
