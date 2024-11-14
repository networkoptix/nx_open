// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "recording_status_helper.h"

#include <chrono>

#include <QtQml/QtQml>

#include <core/misc/schedule_task.h>
#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/math/math.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/system_context.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::api;

namespace {

const core::SvgIconColorer::ThemeSubstitutions kArchiveTheme = {
    {QIcon::Normal, {.primary = "light10"}}
};

const core::SvgIconColorer::ThemeSubstitutions kRecordingTheme = {
    {QIcon::Normal, {.primary = "red_l1"}}
};

NX_DECLARE_COLORIZED_ICON(kArchiveIcon, "16x16/Solid/archive.svg", kArchiveTheme)
NX_DECLARE_COLORIZED_ICON(kNotRecordingIcon, "16x16/Solid/notrecordingnow.svg", kRecordingTheme)
NX_DECLARE_COLORIZED_ICON(kRecordingIcon, "16x16/Solid/recordingnow.svg", kRecordingTheme)

bool hasArchive(const QnVirtualCameraResourcePtr& camera)
{
    auto systemContext = SystemContext::fromResource(camera);
    if (!NX_ASSERT(systemContext))
        return false;

    const auto footageServers =
        systemContext->cameraHistoryPool()->getCameraFootageData(camera, /*filterOnlineServers*/ true);

    return !footageServers.empty();
};

using namespace std::chrono;

static constexpr milliseconds kUpdateInterval = minutes(1);

static constexpr auto kMotionAndObjectsMetadataTypes = RecordingMetadataTypes(
    {RecordingMetadataType::motion, RecordingMetadataType::objects});

static constexpr auto kMotionMetadataTypes = RecordingMetadataTypes(
    RecordingMetadataType::motion);

static constexpr auto kObjectsMetadataTypes = RecordingMetadataTypes(
    RecordingMetadataType::objects);

std::pair<RecordingStatus, RecordingMetadataTypes> currentRecordingMode(
    const QnVirtualCameraResourcePtr& camera)
{
    if (!camera || camera->hasFlags(Qn::virtual_camera))
        return {RecordingStatus::noRecordingNoArchive, {RecordingMetadataType::none}};

    if (camera->isScheduleEnabled())
    {
        const auto dateTime = qnSyncTime->currentDateTime();
        const int dayOfWeek = dateTime.date().dayOfWeek();
        const int seconds = dateTime.time().msecsSinceStartOfDay() / 1000;

        const auto scheduledTasks = camera->getScheduleTasks();
        bool recordScheduled = false;
        for (const auto& task: scheduledTasks)
        {
            const bool isRecording = (task.recordingType != RecordingType::never);
            recordScheduled |= isRecording;
            if (task.dayOfWeek == dayOfWeek && qBetween(task.startTime, seconds, task.endTime + 1)
                && isRecording)
            {
                auto status = RecordingStatus::recordingContinious;
                if (task.recordingType == RecordingType::metadataOnly)
                    status = RecordingStatus::recordingMetadataOnly;
                else if (task.recordingType == RecordingType::metadataAndLowQuality)
                    status = RecordingStatus::recordingMetadataAndLQ;
                return {status, task.metadataTypes};
            }
        }

        if (recordScheduled)
            return {RecordingStatus::recordingScheduled, {RecordingMetadataType::none}};
    }

    return {hasArchive(camera)
        ? RecordingStatus::noRecordingOnlyArchive
        : RecordingStatus::noRecordingNoArchive,
        {RecordingMetadataType::none}};
}

} // namespace

RecordingStatusHelper::RecordingStatusHelper(QObject* parent):
    QObject(parent)
{
    auto timer = new QTimer(this);
    timer->setInterval(kUpdateInterval);
    connect(timer, &QTimer::timeout, this, &RecordingStatusHelper::updateRecordingMode);
    timer->start();
}

QnResource* RecordingStatusHelper::rawResource() const
{
    return m_camera.data();
}

void RecordingStatusHelper::setRawResource(QnResource* value)
{
    setCamera(value
        ? value->toSharedPointer().dynamicCast<QnVirtualCameraResource>()
        : QnVirtualCameraResourcePtr());
}

QnVirtualCameraResourcePtr RecordingStatusHelper::camera() const
{
    return m_camera;
}

void RecordingStatusHelper::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (m_camera == camera)
        return;

    m_connections.reset();
    m_camera = camera;

    if (m_camera)
    {
        m_connections << connect(
            m_camera.data(),
            &QnVirtualCameraResource::statusChanged,
            this,
            &RecordingStatusHelper::updateRecordingMode);
        m_connections << connect(
            m_camera.data(),
            &QnVirtualCameraResource::scheduleTasksChanged,
            this,
            &RecordingStatusHelper::updateRecordingMode);

        if (auto systemContext = SystemContext::fromResource(m_camera); NX_ASSERT(systemContext))
        {
            m_connections << connect(
                systemContext->serverTimeWatcher(),
                &core::ServerTimeWatcher::timeZoneChanged,
                this,
                &RecordingStatusHelper::updateRecordingMode);
        }
    }

    updateRecordingMode();
    emit resourceChanged();
}

QString RecordingStatusHelper::tooltip() const
{
    return tooltip(m_recordingStatus, m_metadataTypes);
}

QString RecordingStatusHelper::shortTooltip() const
{
    return shortTooltip(m_recordingStatus, m_metadataTypes);
}

QString RecordingStatusHelper::qmlIconName() const
{
    return qmlIconName(m_recordingStatus, m_metadataTypes);
}

QIcon RecordingStatusHelper::icon() const
{
    return icon(m_recordingStatus, m_metadataTypes);
}

QString RecordingStatusHelper::tooltip(
    RecordingStatus recordingStatus,
    RecordingMetadataTypes metadataTypes)
{
    switch (recordingStatus)
    {
        case RecordingStatus::noRecordingOnlyArchive:
            return tr("Not recording");
        case RecordingStatus::recordingContinious:
            return tr("Recording everything");
        case RecordingStatus::recordingMetadataOnly:
            switch (metadataTypes)
            {
                case kMotionAndObjectsMetadataTypes:
                    return tr("Recording motion and objects");
                case kMotionMetadataTypes:
                    return tr("Recording motion only");
                case kObjectsMetadataTypes:
                    return tr("Recording objects only");
            }
            break;
        case RecordingStatus::recordingMetadataAndLQ:
            switch (metadataTypes)
            {
                case kMotionAndObjectsMetadataTypes:
                    return tr("Recording motion, objects and low quality");
                case kMotionMetadataTypes:
                    return tr("Recording motion and low quality");
                case kObjectsMetadataTypes:
                    return tr("Recording objects and low quality");
            }
            break;
    }
    return QString();
}

QString RecordingStatusHelper::shortTooltip(
    RecordingStatus recordingStatus,
    RecordingMetadataTypes metadataTypes)
{
    switch (recordingStatus)
    {
        case RecordingStatus::noRecordingOnlyArchive:
            return tr("Not recording");
        case RecordingStatus::recordingContinious:
            return tr("Continuous");
        case RecordingStatus::recordingMetadataOnly:
            switch (metadataTypes)
            {
                case kMotionAndObjectsMetadataTypes:
                    return tr("Motion, Objects");
                case kMotionMetadataTypes:
                    return tr("Motion only");
                case kObjectsMetadataTypes:
                    return tr("Objects only");
            }
            break;
        case RecordingStatus::recordingMetadataAndLQ:
            switch (metadataTypes)
            {
                case kMotionAndObjectsMetadataTypes:
                    return tr("Motion, Objects + Lo-Res");
                case kMotionMetadataTypes:
                    return tr("Motion + Lo-Res");
                case kObjectsMetadataTypes:
                    return tr("Objects + Lo-Res");
            }
            break;
    }
    return QString();
}

QString RecordingStatusHelper::shortTooltip(const QnVirtualCameraResourcePtr& camera)
{
    const auto [recordingStatus, metadataTypes] = currentRecordingMode(camera);
    return shortTooltip(recordingStatus, metadataTypes);
}

QString RecordingStatusHelper::qmlIconName(
    RecordingStatus recordingStatus,
    RecordingMetadataTypes /*metadataTypes*/)
{
    switch (recordingStatus)
    {
        case RecordingStatus::recordingContinious:
        case RecordingStatus::recordingMetadataOnly:
        case RecordingStatus::recordingMetadataAndLQ:
            return "image://skin/20x20/Solid/record_on.svg";
        case RecordingStatus::noRecordingOnlyArchive:
            return "image://skin/20x20/Solid/archive.svg";
        case RecordingStatus::recordingScheduled:
            return "image://skin/20x20/Solid/record_part.svg";
    }
    return QString();
}

QIcon RecordingStatusHelper::icon(
    RecordingStatus recordingStatus,
    RecordingMetadataTypes /*metadataTypes*/)
{
    switch (recordingStatus)
    {
        case RecordingStatus::recordingContinious:
        case RecordingStatus::recordingMetadataOnly:
        case RecordingStatus::recordingMetadataAndLQ:
            return qnSkin->icon(kRecordingIcon);
        case RecordingStatus::noRecordingOnlyArchive:
            return qnSkin->icon(kArchiveIcon);
        case RecordingStatus::recordingScheduled:
            return qnSkin->icon(kNotRecordingIcon);
    }
    return QIcon();
}

QIcon RecordingStatusHelper::icon(const QnVirtualCameraResourcePtr& camera)
{
    const auto [recordingStatus, metadataTypes] = currentRecordingMode(camera);
    return icon(recordingStatus, metadataTypes);
}

void RecordingStatusHelper::registerQmlType()
{
    qmlRegisterType<RecordingStatusHelper>("nx.vms.client.desktop", 1, 0, "RecordingStatusHelper");
}

void RecordingStatusHelper::updateRecordingMode()
{
    if (!m_camera)
        return;

    const auto [newRecordingStatus, newMetadataTypes] = currentRecordingMode(m_camera);

    if (newRecordingStatus == m_recordingStatus && newMetadataTypes == m_metadataTypes)
        return;

    m_metadataTypes = newMetadataTypes;
    m_recordingStatus = newRecordingStatus;

    emit recordingModeChanged();
}

} // namespace nx::vms::client::desktop
