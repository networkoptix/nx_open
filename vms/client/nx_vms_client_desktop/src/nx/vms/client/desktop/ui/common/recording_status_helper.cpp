// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "recording_status_helper.h"

#include <chrono>

#include <QtQml/QtQml>

#include <core/misc/schedule_task.h>
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

using namespace std::chrono;

static constexpr milliseconds kUpdateInterval = minutes(1);

static constexpr auto kMotionAndObjectsMetadataTypes = RecordingMetadataTypes(
    {RecordingMetadataType::motion, RecordingMetadataType::objects});

static constexpr auto kMotionMetadataTypes = RecordingMetadataTypes(
    RecordingMetadataType::motion);

static constexpr auto kObjectsMetadataTypes = RecordingMetadataTypes(
    RecordingMetadataType::objects);

std::pair<RecordingType, RecordingMetadataTypes> currentRecordingMode(
    const QnVirtualCameraResourcePtr& camera)
{
    if (!camera || !camera->isScheduleEnabled())
        return {RecordingType::never, {RecordingMetadataType::none}};

    const auto dateTime = qnSyncTime->currentDateTime();
    const int dayOfWeek = dateTime.date().dayOfWeek();
    const int seconds = dateTime.time().msecsSinceStartOfDay() / 1000;

    const auto scheduledTasks = camera->getScheduleTasks();
    for (const auto& task: scheduledTasks)
    {
        if (task.dayOfWeek == dayOfWeek && qBetween(task.startTime, seconds, task.endTime + 1))
            return {task.recordingType, task.metadataTypes};
    }

    return {RecordingType::never, {RecordingMetadataType::none}};
}

} // namespace

RecordingStatusHelper::RecordingStatusHelper(SystemContext* systemContext, QObject* parent):
    QObject(parent),
    SystemContextAware(systemContext)
{
    initialize();
}

RecordingStatusHelper::RecordingStatusHelper():
    QObject(),
    SystemContextAware(SystemContext::fromQmlContext(this))
{
}

void RecordingStatusHelper::initialize()
{
    if (m_initialized)
        return;

    m_initialized = true;

    connect(
        serverTimeWatcher(),
        &core::ServerTimeWatcher::timeZoneChanged,
        this,
        &RecordingStatusHelper::updateRecordingMode);
}

QnUuid RecordingStatusHelper::cameraId() const
{
    return m_camera ? m_camera->getId() : QnUuid();
}

void RecordingStatusHelper::setCameraId(const QnUuid& id)
{
    initialize();
    setCamera(resourcePool()->getResourceById<QnVirtualCameraResource>(id));
}

QnVirtualCameraResourcePtr RecordingStatusHelper::camera() const
{
    return m_camera;
}

void RecordingStatusHelper::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (m_camera == camera)
        return;

    if (m_updateTimerId >= 0)
        killTimer(m_updateTimerId);

    if (m_camera)
        m_camera->disconnect(this);

    m_camera = camera;

    if (m_camera)
    {
        initialize();

        connect(m_camera.data(), &QnVirtualCameraResource::statusChanged, this,
            &RecordingStatusHelper::updateRecordingMode);
        connect(m_camera.data(), &QnVirtualCameraResource::scheduleTasksChanged, this,
            &RecordingStatusHelper::updateRecordingMode);

        m_updateTimerId = startTimer(kUpdateInterval.count());
    }

    updateRecordingMode();
    emit cameraIdChanged();
}

QString RecordingStatusHelper::tooltip() const
{
    return tooltip(m_recordingType, m_metadataTypes);
}

QString RecordingStatusHelper::shortTooltip() const
{
    return shortTooltip(m_recordingType, m_metadataTypes);
}

QString RecordingStatusHelper::qmlIconName() const
{
    return qmlIconName(m_recordingType, m_metadataTypes);
}

QIcon RecordingStatusHelper::icon() const
{
    return icon(m_recordingType, m_metadataTypes);
}

QString RecordingStatusHelper::tooltip(
    RecordingType recordingType,
    RecordingMetadataTypes metadataTypes)
{
    switch (recordingType)
    {
        case RecordingType::never:
            return tr("Not recording");
        case RecordingType::always:
            return tr("Recording everything");
        case RecordingType::metadataOnly:
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
        case RecordingType::metadataAndLowQuality:
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
    RecordingType recordingType,
    RecordingMetadataTypes metadataTypes)
{
    switch (recordingType)
    {
        case RecordingType::never:
            return tr("Not recording");
        case RecordingType::always:
            return tr("Continuous");
        case RecordingType::metadataOnly:
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
        case RecordingType::metadataAndLowQuality:
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
    const auto [recordingType, metadataTypes] = currentRecordingMode(camera);
    return shortTooltip(recordingType, metadataTypes);
}

QString RecordingStatusHelper::qmlIconName(
    RecordingType recordingType,
    RecordingMetadataTypes metadataTypes)
{
    switch (recordingType)
    {
        case RecordingType::never:
            return "qrc:/skin/item/recording_off.png";
        case RecordingType::always:
            return "qrc:/skin/item/recording.png";
        case RecordingType::metadataOnly:
            switch (metadataTypes)
            {
                case kMotionAndObjectsMetadataTypes:
                    return "qrc:/skin/item/recording_motion_objects.png";
                case kMotionMetadataTypes:
                    return "qrc:/skin/item/recording_motion.png";
                case kObjectsMetadataTypes:
                    return "qrc:/skin/item/recording_objects.png";
            }
            break;
        case RecordingType::metadataAndLowQuality:
            switch (metadataTypes)
            {
                case kMotionAndObjectsMetadataTypes:
                    return "qrc:/skin/item/recording_motion_objects_lq.png";
                case kMotionMetadataTypes:
                    return "qrc:/skin/item/recording_motion_lq.png";
                case kObjectsMetadataTypes:
                    return "qrc:/skin/item/recording_objects_lq.png";
            }
            break;
    }
    return QString();
}

QIcon RecordingStatusHelper::icon(
    RecordingType recordingType,
    RecordingMetadataTypes metadataTypes)
{
    switch (recordingType)
    {
        case RecordingType::never:
            return qnSkin->icon("item/recording_off.png");
        case RecordingType::always:
            return qnSkin->icon("item/recording.png");
        case RecordingType::metadataOnly:
            switch (metadataTypes)
            {
                case kMotionAndObjectsMetadataTypes:
                    return qnSkin->icon("item/recording_motion_objects.png");
                case kMotionMetadataTypes:
                    return qnSkin->icon("item/recording_motion.png");
                case kObjectsMetadataTypes:
                    return qnSkin->icon("item/recording_objects.png");
            }
            break;
        case RecordingType::metadataAndLowQuality:
            switch (metadataTypes)
            {
                case kMotionAndObjectsMetadataTypes:
                    return qnSkin->icon("item/recording_motion_objects_lq.png");
                case kMotionMetadataTypes:
                    return qnSkin->icon("item/recording_motion_lq.png");
                case kObjectsMetadataTypes:
                    return qnSkin->icon("item/recording_objects_lq.png");
            }
            break;
    }
    return QIcon();
}

QIcon RecordingStatusHelper::icon(const QnVirtualCameraResourcePtr& camera)
{
    const auto [recordingType, metadataTypes] = currentRecordingMode(camera);
    return icon(recordingType, metadataTypes);
}

void RecordingStatusHelper::registerQmlType()
{
    qmlRegisterType<RecordingStatusHelper>("nx.client.desktop", 1, 0, "RecordingStatusHelper");
}

void RecordingStatusHelper::updateRecordingMode()
{
    const auto [newRecordingType, newMetadataTypes] = currentRecordingMode(m_camera);

    if (newRecordingType == m_recordingType && newMetadataTypes == m_metadataTypes)
        return;

    m_recordingType = newRecordingType;
    m_metadataTypes = newMetadataTypes;

    emit recordingModeChanged();
}

void RecordingStatusHelper::timerEvent(QTimerEvent* event)
{
    if (event->timerId() != m_updateTimerId)
        return;

    updateRecordingMode();
}

} // namespace nx::vms::client::desktop
