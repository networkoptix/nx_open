// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/resource/camera_history.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/skin/svg_icon_colorer.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/test_support/test_context.h>
#include <nx/vms/client/desktop/ui/common/recording_status_helper.h>
#include <nx/vms/common/test_support/test_context.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::desktop {
namespace test {

const core::SvgIconColorer::ThemeSubstitutions kArchiveTheme = {
    {QIcon::Normal, {.primary = "light10"}}};

const core::SvgIconColorer::ThemeSubstitutions kRecordingTheme = {
    {QIcon::Normal, {.primary = "red_l1"}}};

NX_DECLARE_COLORIZED_ICON(kArchiveIcon, "16x16/Solid/archive.svg", kArchiveTheme)
NX_DECLARE_COLORIZED_ICON(kNotRecordingIcon, "16x16/Solid/notrecordingnow.svg", kRecordingTheme)
NX_DECLARE_COLORIZED_ICON(kRecordingIcon, "16x16/Solid/recordingnow.svg", kRecordingTheme)

class RecordingStatusHelperTest: public ContextBasedTest
{
    RecordingStatusHelper m_recordingStatusHelper;
    nx::CameraResourceStubPtr m_camera;

public:
    RecordingStatusHelperTest()
    {
        m_camera = createCamera();
        m_camera->addToSystemContext(appContext()->currentSystemContext());

        m_recordingStatusHelper.setCamera(m_camera);
    }

    void thenIconIs(const QIcon& icon)
    {
        QByteArray actualIcon;
        QDataStream stream1(&actualIcon, QIODevice::WriteOnly);
        stream1 << m_recordingStatusHelper.icon();

        QByteArray expectedIcon;
        QDataStream stream2(&expectedIcon, QIODevice::WriteOnly);
        stream2 << icon;

        ASSERT_EQ(actualIcon, expectedIcon);
    }

    void thenIconPathIs(const QString& path)
    {
        ASSERT_EQ(m_recordingStatusHelper.qmlIconName(), path);
    }

    void whenRecordingEnabled()
    {
        api::CameraAttributesData attributes;
        attributes.scheduleEnabled = true;
        m_camera->setUserAttributesAndNotify(attributes);
    }

    void whenRecordingTodayIs(nx::vms::api::RecordingType recordingType)
    {
        auto tasks = m_camera->getScheduleTasks();
        QnSyncTime syncTime;
        auto dateTime = syncTime.currentDateTime();
        const int dayOfWeek = dateTime.date().dayOfWeek();
        tasks[dayOfWeek - 1].recordingType = recordingType; // Vectors starts from 0, days from 1
        m_camera->setScheduleTasks(tasks);
    }

    void givenRecordingIsSet(Qn::RecordingType recordingType)
    {
        nx::vms::api::CameraScheduleTaskDataList tasks = nx::vms::common::defaultSchedule(30);
        for (auto& t : tasks)
            t.recordingType = recordingType;

        m_camera->setScheduleTasks(tasks);
    }

    void givenArchiveExists()
    {
        auto server = addServer();
        server->setStatus(api::ResourceStatus::online);
        m_camera->setParentId(server->getId());

        systemContext()->cameraHistoryPool()->setServerFootageData(
            server->getId(), {m_camera->getId()});
    }
};

TEST_F(RecordingStatusHelperTest, noIcon)
{
    thenIconIs(QIcon());
    thenIconPathIs(QString());
}

TEST_F(RecordingStatusHelperTest, recordingIconNotEnabled)
{
    givenRecordingIsSet(nx::vms::api::RecordingType::always);
    thenIconIs(QIcon());
    thenIconPathIs(QString());
}

TEST_F(RecordingStatusHelperTest, recordingIcon)
{
    whenRecordingEnabled();
    givenRecordingIsSet(nx::vms::api::RecordingType::always);
    thenIconIs(qnSkin->icon(kRecordingIcon));
    thenIconPathIs(QString("image://skin/20x20/Solid/record_on.svg"));
}

TEST_F(RecordingStatusHelperTest, recordingIconLoRes)
{
    whenRecordingEnabled();
    givenRecordingIsSet(nx::vms::api::RecordingType::metadataAndLowQuality);
    thenIconIs(qnSkin->icon(kRecordingIcon));
    thenIconPathIs(QString("image://skin/20x20/Solid/record_on.svg"));
}

TEST_F(RecordingStatusHelperTest, recordingIconMetadataOnly)
{
    whenRecordingEnabled();
    givenRecordingIsSet(nx::vms::api::RecordingType::metadataOnly);
    thenIconIs(qnSkin->icon(kRecordingIcon));
    thenIconPathIs(QString("image://skin/20x20/Solid/record_on.svg"));
}

TEST_F(RecordingStatusHelperTest, recordingScheduled)
{
    whenRecordingEnabled();
    givenRecordingIsSet(nx::vms::api::RecordingType::always);
    whenRecordingTodayIs(nx::vms::api::RecordingType::never);

    thenIconIs(qnSkin->icon(kNotRecordingIcon));
    thenIconPathIs(QString("image://skin/20x20/Solid/record_part.svg"));
}

TEST_F(RecordingStatusHelperTest, archiveIcon)
{
    givenArchiveExists();
    givenRecordingIsSet(nx::vms::api::RecordingType::never);
    thenIconIs(qnSkin->icon(kArchiveIcon));
    thenIconPathIs(QString("image://skin/20x20/Solid/archive.svg"));
}

TEST_F(RecordingStatusHelperTest, archiveIconAndRecording)
{
    whenRecordingEnabled();
    givenRecordingIsSet(nx::vms::api::RecordingType::never);
    givenArchiveExists();
    whenRecordingTodayIs(nx::vms::api::RecordingType::always);
    thenIconIs(qnSkin->icon(kRecordingIcon));
    thenIconPathIs(QString("image://skin/20x20/Solid/record_on.svg"));
}

TEST_F(RecordingStatusHelperTest, archiveIconAndScheduled)
{
    whenRecordingEnabled();
    givenRecordingIsSet(nx::vms::api::RecordingType::metadataOnly);
    givenArchiveExists();
    whenRecordingTodayIs(nx::vms::api::RecordingType::never);
    thenIconIs(qnSkin->icon(kNotRecordingIcon));
    thenIconPathIs(QString("image://skin/20x20/Solid/record_part.svg"));
}

} // namespace test
} // namespace nx::vms::client::desktop
