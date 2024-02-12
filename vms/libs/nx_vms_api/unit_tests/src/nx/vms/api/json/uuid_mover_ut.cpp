// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/uuid.h>
#include <nx/fusion/serialization/json.h>
#include <nx/vms/api/json/uuid_mover.h>

namespace nx::vms::api::json::test {

TEST(UuidMover, moveTwoIds)
{
    static const json::UuidMover::FieldMap kFieldsToMove = {
        {"id", {"general", "id"}},
        {"serverId", {"general", "parentId"}}
    };
    QJsonObject general;
    general.insert("key", "value");
    QJsonObject value;
    value.insert("general", general);
    const QString id = nx::Uuid::createUuid().toString();
    value.insert("id", id);
    const QString parentId = nx::Uuid::createUuid().toString();
    value.insert("serverId", parentId);
    json::UuidMover mover;
    QnJsonContext ctx;
    ASSERT_TRUE(mover.move(kFieldsToMove, value, &ctx));
    QJsonValue result = mover.result();
    ASSERT_EQ(result.toObject()["general"].toObject()["id"].toString(), id);
    ASSERT_EQ(result.toObject()["general"].toObject()["parentId"].toString(), parentId);
    ASSERT_EQ(result.toObject()["general"].toObject()["key"].toString(), "value");
}

TEST(UuidMover, createField)
{
    static const json::UuidMover::FieldMap kFieldsToMove = {
        {"id", {"general", "id"}}
    };
    QJsonObject value;
    const QString id = nx::Uuid::createUuid().toString();
    value.insert("id", id);
    json::UuidMover mover;
    QnJsonContext ctx;
    ASSERT_TRUE(mover.move(kFieldsToMove, value, &ctx));
    QJsonValue result = mover.result();
    ASSERT_EQ(result.toObject()["general"].toObject()["id"].toString(), id);
}

} // namespace nx::vms::api::json::test
