// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/rest/array_orderer.h>
#include <nx/utils/json.h>
#include <nx/utils/uuid.h>

namespace nx::network::rest::json::test {

using namespace nx::utils::json;

TEST(ArrayOrderer, ByValues)
{
    QJsonArray result;
    result.append(QJsonObject({{"nested", QJsonObject({{"name", "2"}})}}));
    result.append(QJsonObject({{"nested", QJsonObject({{"name", "1"}})}}));
    ArrayOrderer orderer({"nested.name"});
    orderer(&result);

    EXPECT_EQ(getString(getObject(asObject(result[0]), "nested"), "name"), "1");
}

TEST(ArrayOrderer, ByValuesWithArray)
{
    QJsonArray nestedArray;
    nestedArray.append(QJsonObject({{"id", 2}, {"name", "2"}}));
    nestedArray.append(QJsonObject({{"id", 1}, {"name", "1"}}));
    QJsonArray result;
    result.append(QJsonObject({{"nested", QJsonObject({{"name", "2"}, {"array", nestedArray}})}}));
    result.append(QJsonObject({{"nested", QJsonObject({{"name", "1"}})}}));

    ArrayOrderer orderer({"nested.array[].name"});
    orderer(&result);

    EXPECT_EQ(getString(getObject(asObject(result[0]), "nested"), "name"), "2");

    ArrayOrderer::FieldScope scope1("nested", &orderer);
    ArrayOrderer::FieldScope scope2("array[]", &orderer);
    orderer(&nestedArray);
    EXPECT_EQ(getDouble(asObject(nestedArray[0]), "id"), 1);
}

TEST(ArrayOrderer, MissingValue)
{
    QJsonArray result;
    result.append(QJsonObject({{"id", 3}}));
    result.append(QJsonObject({{"id", 2}, {"name", "2"}}));
    result.append(QJsonObject({{"id", 0}, {"name", "0"}}));
    result.append(QJsonObject({{"id", 4}}));
    result.append(QJsonObject({{"id", 1}, {"name", "1"}}));

    ArrayOrderer orderer({"name"});
    orderer(&result);
    for (int i = 0; i < result.size(); ++i)
        EXPECT_EQ(getDouble(asObject(result[i]), "id"), i);
}

} // namespace nx::network::rest::json::test
