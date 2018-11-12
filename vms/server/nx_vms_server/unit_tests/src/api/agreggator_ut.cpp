#include <gtest/gtest.h>

#include <nx/utils/log/log.h>

#include "test_api_requests.h"

namespace nx {
namespace test {

TEST(QnJsonAggregatorRestHandler, Users)
{
    const auto kUsers = lit("ec2/getUsers");
    const auto kPredefinedRoles = lit("ec2/getPredefinedRoles");
    const auto kUserRoles = lit("ec2/getUserRoles");
    const auto kAggregation = lit("api/aggregator");

    MediaServerLauncher launcher;
    ASSERT_TRUE(launcher.start());

    QJsonDocument users;
    NX_TEST_API_GET(&launcher, kUsers, &users);
    ASSERT_TRUE(users.isArray());

    QJsonDocument predefinedRoles;
    NX_TEST_API_GET(&launcher, kPredefinedRoles, &predefinedRoles);
    ASSERT_TRUE(predefinedRoles.isArray());

    QJsonDocument userRoles;
    NX_TEST_API_GET(&launcher, kUserRoles, &userRoles);
    ASSERT_TRUE(userRoles.isArray());

    QUrlQuery aggregationQuery;
    for (const auto& q: {kUsers, kPredefinedRoles, kUserRoles})
        aggregationQuery.addQueryItem("exec_cmd", q);

    QJsonDocument aggregation;
    NX_TEST_API_GET(&launcher, lm("%1?%2").args(kAggregation, aggregationQuery), &aggregation);
    ASSERT_TRUE(aggregation.isObject());

    const auto response = aggregation.object();
    EXPECT_EQ("0", response.value("error").toString());

    const auto object = response.value("reply").toObject();
    const auto expectJsonArray = [&](const QJsonDocument& document, const QString& key)
        {
            const auto expected = QJson::serialized(document.array());
            const auto actual = QJson::serialized(object.value(key).toArray());
            EXPECT_EQ(expected, actual);
        };

    ASSERT_EQ(3, object.size());
    expectJsonArray(users, kUsers);
    expectJsonArray(predefinedRoles, kPredefinedRoles);
    expectJsonArray(userRoles, kUserRoles);
}

} // namespace test
} // namespace nx
