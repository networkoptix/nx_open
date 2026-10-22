// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>

#include <gtest/gtest.h>

#include <QtCore/QTimeZone>

#include <core/resource/camera_history.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_property_key.h>
#include <core/resource_management/resource_pool.h>
#include <nx/reflect/json.h>
#include <nx/vms/api/data/server_timezone_information.h>
#include <nx/vms/client/core/camera/recording_status_helper.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/skin/svg_icon_colorer.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/test_support/test_context.h>
#include <nx/vms/common/test_support/test_context.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::desktop {
namespace test {

using namespace std::chrono;

const core::SvgIconColorer::ThemeSubstitutions kArchiveTheme = {
    {QIcon::Normal, {.primary = "light10"}}};

const core::SvgIconColorer::ThemeSubstitutions kRecordingTheme = {
    {QIcon::Normal, {.primary = "red_l1"}}};

NX_DECLARE_COLORIZED_ICON(kArchiveIcon, "16x16/Solid/archive.svg", kArchiveTheme)
NX_DECLARE_COLORIZED_ICON(kNotRecordingIcon, "16x16/Solid/notrecordingnow.svg", kRecordingTheme)
NX_DECLARE_COLORIZED_ICON(kRecordingIcon, "16x16/Solid/recordingnow.svg", kRecordingTheme)

class RecordingStatusHelperTest: public ContextBasedTest
{
    core::RecordingStatusHelper m_recordingStatusHelper;
    nx::CameraResourceStubPtr m_camera;

public:
    RecordingStatusHelperTest()
    {
        m_camera = addCamera();
        m_camera->setStatus(api::ResourceStatus::recording);

        m_recordingStatusHelper.setCamera(m_camera);
    }

    void thenIconIs(const QIcon& icon)
    {
        QByteArray actualIcon;
        QDataStream stream1(&actualIcon, QIODevice::WriteOnly);
        stream1 << m_recordingStatusHelper.smallIcon();

        QByteArray expectedIcon;
        QDataStream stream2(&expectedIcon, QIODevice::WriteOnly);
        stream2 << icon;

        ASSERT_EQ(actualIcon, expectedIcon);
    }

    void thenIconPathIs(const QString& path)
    {
        ASSERT_EQ(m_recordingStatusHelper.qmlIconName(), path);
    }

    void thenRecordingEnabledIs(bool value)
    {
        ASSERT_EQ(m_recordingStatusHelper.recordingEnabled(), value);
    }

    void thenShortTooltipIs(const QString& value)
    {
        ASSERT_EQ(m_recordingStatusHelper.shortTooltip(), value);
    }

    void whenCameraIsRecording() { m_camera->setStatus(api::ResourceStatus::recording); }

    void whenRecordingEnabled() { m_camera->setScheduleEnabled(true); }

    void whenStorageDisabled()
    {
        m_camera->setStatus(api::ResourceStatus::online);
    }

    void whenRecordingDisabled() { m_camera->setScheduleEnabled(false); }

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

    void givenServerTimeZone(seconds offsetFromUtc)
    {
        auto server = addServer();
        server->setStatus(api::ResourceStatus::online);

        nx::vms::api::ServerTimeZoneInformation timeZoneInfo;
        timeZoneInfo.timeZoneOffsetMs = offsetFromUtc;
        server->setProperty(ResourcePropertyKey::Server::kTimeZoneInformation,
            QString::fromStdString(nx::reflect::json::serialize(timeZoneInfo)));

        m_camera->setParentId(server->getId());
    }

    void givenRecordingOnlyOn(int dayOfWeek, nx::vms::api::RecordingType recordingType)
    {
        auto tasks = nx::vms::common::defaultSchedule(30);
        for (auto& task: tasks)
        {
            task.recordingType =
                task.dayOfWeek == dayOfWeek ? recordingType : nx::vms::api::RecordingType::never;
        }

        m_camera->setScheduleTasks(tasks);
    }
};

TEST_F(RecordingStatusHelperTest, noIcon)
{
    thenIconIs(QIcon());
    thenIconPathIs(QString());
    thenRecordingEnabledIs(false);
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
    thenRecordingEnabledIs(true);
}

TEST_F(RecordingStatusHelperTest, storageDisabled)
{
    whenRecordingEnabled();
    givenRecordingIsSet(nx::vms::api::RecordingType::always);
    whenStorageDisabled();
    thenIconIs(qnSkin->icon(kNotRecordingIcon));
    thenIconPathIs(QString("image://skin/20x20/Solid/record_part.svg"));
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
    whenStorageDisabled(); //< If never, status should be online.
    thenIconIs(qnSkin->icon(kNotRecordingIcon));
    thenIconPathIs(QString("image://skin/20x20/Solid/record_part.svg"));
    thenRecordingEnabledIs(true);
}

TEST_F(RecordingStatusHelperTest, archiveIcon)
{
    givenArchiveExists();
    givenRecordingIsSet(nx::vms::api::RecordingType::never);
    thenIconIs(qnSkin->icon(kArchiveIcon));
    thenIconPathIs(QString("image://skin/20x20/Solid/archive.svg"));
    thenRecordingEnabledIs(false);
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
    whenStorageDisabled(); //< If never, status should be online.
    thenIconIs(qnSkin->icon(kNotRecordingIcon));
    thenIconPathIs(QString("image://skin/20x20/Solid/record_part.svg"));
}

TEST_F(RecordingStatusHelperTest, archiveAppearanceUpdatesIcon)
{
    givenRecordingIsSet(nx::vms::api::RecordingType::never);
    thenIconIs(QIcon());
    givenArchiveExists();
    thenIconIs(qnSkin->icon(kArchiveIcon));
    thenIconPathIs(QString("image://skin/20x20/Solid/archive.svg"));
}

TEST_F(RecordingStatusHelperTest, scheduleDisablingUpdatesIcon)
{
    whenRecordingEnabled();
    givenRecordingIsSet(nx::vms::api::RecordingType::always);
    thenIconIs(qnSkin->icon(kRecordingIcon));
    whenRecordingDisabled();
    thenIconIs(QIcon());
    thenIconPathIs(QString());
}

TEST_F(RecordingStatusHelperTest, recordingNotCoveredBySchedule)
{
    whenRecordingEnabled();
    givenRecordingIsSet(nx::vms::api::RecordingType::never);
    whenCameraIsRecording(); //< E.g. recording is started by an event rule.
    thenIconIs(qnSkin->icon(kRecordingIcon));
    thenIconPathIs(QString("image://skin/20x20/Solid/record_on.svg"));
    thenRecordingEnabledIs(true);
    thenShortTooltipIs("Continuous");
}

TEST_F(RecordingStatusHelperTest, recordingStatusIgnoredWhenScheduleDisabled)
{
    givenRecordingIsSet(nx::vms::api::RecordingType::always);
    whenCameraIsRecording(); //< E.g. the server has not stopped recording yet.
    thenIconIs(QIcon());
    thenIconPathIs(QString());
    thenRecordingEnabledIs(false);

    givenArchiveExists();
    thenIconIs(qnSkin->icon(kArchiveIcon));
    thenIconPathIs(QString("image://skin/20x20/Solid/archive.svg"));
    thenRecordingEnabledIs(false);
}

TEST_F(RecordingStatusHelperTest, scheduleUsesServerTimeZone)
{
    // Choose a server time zone where the current day of week differs from the local one. Dates
    // in UTC+14 and UTC-12 always differ, so at least one of them differs from the local date.
    const auto now = QDateTime::currentDateTime();
    seconds serverOffset = 14h;
    if (now.toTimeZone(QTimeZone::fromDurationAheadOfUtc(serverOffset)).date() == now.date())
        serverOffset = -12h;
    const int serverDayOfWeek =
        now.toTimeZone(QTimeZone::fromDurationAheadOfUtc(serverOffset)).date().dayOfWeek();
    ASSERT_NE(serverDayOfWeek, now.date().dayOfWeek());

    givenServerTimeZone(serverOffset);
    whenRecordingEnabled();
    whenCameraIsRecording();
    givenRecordingOnlyOn(serverDayOfWeek, nx::vms::api::RecordingType::metadataOnly);

    // With the local time zone the current slot would be "never", resulting in "Continuous".
    thenShortTooltipIs("Motion only");
}

} // namespace test
} // namespace nx::vms::client::desktop
