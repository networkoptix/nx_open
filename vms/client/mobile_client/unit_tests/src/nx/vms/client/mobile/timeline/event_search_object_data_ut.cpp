// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <memory>

#include <gtest/gtest.h>

#include <QtGui/QStandardItemModel>

#include <analytics/db/analytics_db_types.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/mobile/timeline/object_data_adapter.h>

#include "../test_support/test_context.h"

namespace nx::vms::client::mobile::timeline::test {

using namespace std::chrono;

class EventSearchObjectDataTest: public mobile::test::ApplicationContextBasedTest
{
public:
    virtual void SetUp() override
    {
        camera = addCamera();
        model = std::make_unique<QStandardItemModel>();
        for (const auto& name: {"First", "Second"})
            model->appendRow(makeItem(name));
    }

    virtual void TearDown() override
    {
        model.reset();
        camera.reset();
        mobile::test::ApplicationContextBasedTest::TearDown();
    }

    QStandardItem* makeItem(const QString& name) const
    {
        const auto item = new QStandardItem();
        item->setData(QVariant::fromValue<QnResourcePtr>(camera), core::ResourceRole);
        item->setData(QVariant::fromValue(makeBookmark(name)), core::CameraBookmarkRole);
        return item;
    }

    common::CameraBookmark makeBookmark(const QString& name) const
    {
        common::CameraBookmark bookmark;
        bookmark.guid = nx::Uuid::createUuid();
        bookmark.cameraId = camera->getId();
        bookmark.name = name;
        bookmark.startTimeMs = 100000ms;
        bookmark.durationMs = 5s;
        return bookmark;
    }

    void setBookmark(int row, const QString& name)
    {
        model->setData(model->index(row, 0),
            QVariant::fromValue(makeBookmark(name)),
            core::CameraBookmarkRole);
    }

    QnVirtualCameraResourcePtr camera;
    std::unique_ptr<QStandardItemModel> model;
};

TEST_F(EventSearchObjectDataTest, create)
{
    const std::unique_ptr<EventSearchObjectData> data(
        ObjectDataAdapter().create(model->index(0, 0)));

    ASSERT_TRUE(data);
    ASSERT_EQ(data->resource(), camera);
    ASSERT_EQ(data->data().title, "First");
}

TEST_F(EventSearchObjectDataTest, noObjectData)
{
    const auto item = new QStandardItem();
    item->setData(QVariant::fromValue<QnResourcePtr>(camera), core::ResourceRole);
    model->appendRow(item);

    ASSERT_FALSE(ObjectDataAdapter().create(model->index(2, 0)));
}

TEST_F(EventSearchObjectDataTest, rowChanged)
{
    EventSearchObjectData data(model->index(0, 0));
    int changeCount = 0;
    QObject::connect(&data, &EventSearchObjectData::dataChanged, [&]() { ++changeCount; });

    setBookmark(0, "Changed");
    ASSERT_EQ(changeCount, 1);
    ASSERT_EQ(data.data().title, "Changed");
}

TEST_F(EventSearchObjectDataTest, otherRowChanged)
{
    EventSearchObjectData data(model->index(0, 0));
    int changeCount = 0;
    QObject::connect(&data, &EventSearchObjectData::dataChanged, [&]() { ++changeCount; });

    setBookmark(1, "Changed");
    ASSERT_EQ(changeCount, 0);
    ASSERT_EQ(data.data().title, "First");
}

TEST_F(EventSearchObjectDataTest, unrelatedRoleChanged)
{
    EventSearchObjectData data(model->index(0, 0));
    int changeCount = 0;
    QObject::connect(&data, &EventSearchObjectData::dataChanged, [&]() { ++changeCount; });

    model->setData(model->index(0, 0), "Tooltip", Qt::ToolTipRole);
    ASSERT_EQ(changeCount, 0);
}

TEST_F(EventSearchObjectDataTest, wholeRowChanged)
{
    EventSearchObjectData data(model->index(0, 0));
    int changeCount = 0;
    QObject::connect(&data, &EventSearchObjectData::dataChanged, [&]() { ++changeCount; });

    model->blockSignals(true);
    setBookmark(0, "Changed");
    model->blockSignals(false);

    const auto index = model->index(0, 0);
    emit model->dataChanged(index, index); //< No roles means all roles.
    ASSERT_EQ(changeCount, 1);
    ASSERT_EQ(data.data().title, "Changed");
}

TEST_F(EventSearchObjectDataTest, rowRemoved)
{
    EventSearchObjectData data(model->index(0, 0));
    model->removeRow(0);

    setBookmark(0, "Changed");
    ASSERT_EQ(data.data().title, "First");
}

TEST_F(EventSearchObjectDataTest, analyticsTrackRow)
{
    const auto makeTrack = [this](milliseconds duration)
    {
        nx::analytics::db::ObjectTrack track;
        track.id = nx::Uuid::createUuid();
        track.deviceId = camera->getId();
        track.firstAppearanceTimeUs = microseconds(100000ms).count();
        track.lastAppearanceTimeUs = microseconds(100000ms + duration).count();
        return track;
    };

    const auto item = new QStandardItem();
    item->setData(QVariant::fromValue<QnResourcePtr>(camera), core::ResourceRole);
    item->setData(QVariant::fromValue(makeTrack(5s)), core::ObjectTrackRole);
    model->appendRow(item);

    EventSearchObjectData data(model->index(2, 0));
    ASSERT_TRUE(data.isValid());
    ASSERT_EQ(data.data().durationMs, 5000);

    int changeCount = 0;
    QObject::connect(&data, &EventSearchObjectData::dataChanged, [&]() { ++changeCount; });

    model->setData(model->index(2, 0), QVariant::fromValue(makeTrack(7s)), core::ObjectTrackRole);
    ASSERT_EQ(changeCount, 1);
    ASSERT_EQ(data.data().durationMs, 7000);
}

TEST_F(EventSearchObjectDataTest, rowLosesBookmark)
{
    // The last known data is kept.
    EventSearchObjectData data(model->index(0, 0));
    model->setData(model->index(0, 0), QVariant(), core::CameraBookmarkRole);

    ASSERT_TRUE(data.isValid());
    ASSERT_EQ(data.data().title, "First");
}

} // namespace nx::vms::client::mobile::timeline::test
