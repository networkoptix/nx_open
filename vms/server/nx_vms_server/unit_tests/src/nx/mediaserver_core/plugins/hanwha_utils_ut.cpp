#include <gtest/gtest.h>

#include <plugins/resource/hanwha/hanwha_utils.h>

using namespace nx::vms::server::plugins;

namespace {

HanwhaVideoProfile makeProfile(int number, const QString& name)
{
    HanwhaVideoProfile profile;
    profile.number = number;
    profile.name = name;

    return profile;
}

struct ProfileTestCase
{
    ProfileTestCase(
        const QString& applicationName,
        Qn::ConnectionRole role,
        int profileNumber)
        :
        applicationName(applicationName),
        role(role),
        profileNumber(profileNumber)
    {
    }

    QString applicationName;
    Qn::ConnectionRole role;
    int profileNumber;
};

} // namespace

TEST(HanwhaUtils, findProfile)
{
    static const HanwhaProfileMap kProfileMap = {
        {1, makeProfile(1, "MOBILE")},
        {2, makeProfile(2, "H264")},
        {3, makeProfile(3, "H265")},

        {4, makeProfile(4, "WitnPrimary")},
        {5, makeProfile(5, "WitnessPrimary")},
        {6, makeProfile(6, "WitnesPrimary")},
        {7, makeProfile(7, "WitnessSecondary")},
        {8, makeProfile(8, "WitnesSecondary")},
        {9, makeProfile(9, "WitnSecondary")},

        {16, makeProfile(16, "SomeLongNameSecondary")},
        {10, makeProfile(10, "WAVPrimary")},
        {11, makeProfile(11, "WAVEPrimary")},
        {12, makeProfile(12, "WAPrimary")},
        {13, makeProfile(13, "WAVESecondary")},
        {14, makeProfile(14, "WAVSecondary")},

        {15, makeProfile(15, "SomeLongNamePrimary")},
    };

    static const QString kNxWitness("Nx Witness");
    static const QString kWave("Wisenet WAVE");

    static const std::vector<ProfileTestCase> kTestCases = {
        {kNxWitness, Qn::ConnectionRole::CR_LiveVideo, 5},
        {kNxWitness, Qn::ConnectionRole::CR_SecondaryLiveVideo, 7},
        {kWave, Qn::ConnectionRole::CR_LiveVideo, 11},
        {kWave, Qn::ConnectionRole::CR_SecondaryLiveVideo, 13}
    };

    for (const auto& testCase : kTestCases)
    {
        const auto profile = findProfile(kProfileMap, testCase.role, testCase.applicationName);
        ASSERT_NE(profile, boost::none);
        ASSERT_EQ(profile->number, testCase.profileNumber);
    }
}
