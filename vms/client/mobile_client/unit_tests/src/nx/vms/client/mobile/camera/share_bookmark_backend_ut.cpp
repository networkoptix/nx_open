// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <memory>

#include <gtest/gtest.h>

#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <nx/vms/client/mobile/camera/share_bookmark_backend.h>
#include <nx/vms/client/mobile/timeline/bookmark_data_helper.h>

#include "../test_support/test_context.h"

namespace nx::vms::client::mobile::test {

using namespace std::chrono;

class ShareBookmarkBackendTest: public ApplicationContextBasedTest
{
protected:
    virtual void SetUp() override
    {
        m_camera = addCamera();
        m_backend = std::make_unique<ShareBookmarkBackend>();
        m_backend->setRawResource(m_camera.get());
    }

    virtual void TearDown() override
    {
        m_backend.reset();
        m_camera.reset();
        ApplicationContextBasedTest::TearDown();
    }

    timeline::ObjectData makeBookmarkData(
        const nx::Uuid& bookmarkId, const QString& name, milliseconds duration) const
    {
        common::CameraBookmark bookmark;
        bookmark.guid = bookmarkId;
        bookmark.creatorId = nx::Uuid::createUuid();
        bookmark.cameraId = m_camera->getId();
        bookmark.name = name;
        bookmark.startTimeMs = 100000ms;
        bookmark.durationMs = duration;
        return timeline::BookmarkDataHelper::makeObjectData(bookmark, m_camera);
    }

    void givenObject(const timeline::ObjectData& data)
    {
        m_backend->setObjectData(QVariant::fromValue(data));
    }

    void givenEditedBookmarkName(const QString& name) { m_backend->setBookmarkName(name); }

    void whenObjectAssigned(const timeline::ObjectData& data)
    {
        m_backend->setObjectData(QVariant::fromValue(data));
    }

    void thenBookmarkNameIs(const QString& name) { ASSERT_EQ(m_backend->bookmarkName(), name); }

private:
    QnVirtualCameraResourcePtr m_camera;
    std::unique_ptr<ShareBookmarkBackend> m_backend;
};

TEST_F(ShareBookmarkBackendTest, sameBookmarkUpdateKeepsEdits)
{
    // QML re-assigns the object data on every update of the object.
    const auto bookmarkId = nx::Uuid::createUuid();
    givenObject(makeBookmarkData(bookmarkId, "Name", /*duration*/ 5s));
    givenEditedBookmarkName("Edited");
    whenObjectAssigned(makeBookmarkData(bookmarkId, "Name", /*duration*/ 10s));
    thenBookmarkNameIs("Edited");
}

TEST_F(ShareBookmarkBackendTest, sameAnalyticsObjectUpdateKeepsEdits)
{
    // QML re-assigns the object data on every update of the object (e.g. of an ongoing analytics
    // track). An analytics object is converted to a new bookmark with a null id, which is
    // simulated by a null id bookmark. Analytics object data can't be used directly: its
    // conversion requires `UserWatcher`, which is currently not easily mockable.
    givenObject(makeBookmarkData(/*bookmarkId*/ {}, "Name", /*duration*/ 5s));
    givenEditedBookmarkName("Edited");
    whenObjectAssigned(makeBookmarkData(/*bookmarkId*/ {}, "Name", /*duration*/ 10s));
    thenBookmarkNameIs("Edited");
}

TEST_F(ShareBookmarkBackendTest, anotherObjectResetsEdits)
{
    givenObject(makeBookmarkData(nx::Uuid::createUuid(), "Name", /*duration*/ 5s));
    givenEditedBookmarkName("Edited");
    whenObjectAssigned(makeBookmarkData(nx::Uuid::createUuid(), "Another", /*duration*/ 5s));
    thenBookmarkNameIs("Another");
}

} // namespace nx::vms::client::mobile::test
