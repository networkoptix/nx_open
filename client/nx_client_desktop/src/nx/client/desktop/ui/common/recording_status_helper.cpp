#include "recording_status_helper.h"

#include <chrono>

#include <QtQml/QtQml>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/style/skin.h>
#include <utils/common/synctime.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

using namespace std::chrono;

static constexpr milliseconds kUpdateInterval = minutes(1);

} // namespace

RecordingStatusHelper::RecordingStatusHelper(QObject* parent):
    QObject(parent),
    QnWorkbenchContextAware(
        parent ? parent : this,
        parent
            ? QnWorkbenchContextAware::InitializationMode::instant
            : QnWorkbenchContextAware::InitializationMode::qmlContext)
{
}

QnUuid RecordingStatusHelper::cameraId() const
{
    return m_camera ? m_camera->getId() : QnUuid();
}

void RecordingStatusHelper::setCameraId(const QnUuid& id)
{
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

    m_camera = camera;

    emit cameraIdChanged();

    if (!m_camera)
        return;

    connect(
        context()->instance<QnWorkbenchServerTimeWatcher>(),
        &QnWorkbenchServerTimeWatcher::displayOffsetsChanged,
        this,
        &RecordingStatusHelper::updateRecordingMode);
    connect(m_camera.data(), &QnVirtualCameraResource::statusChanged, this,
        &RecordingStatusHelper::updateRecordingMode);
    connect(m_camera.data(), &QnVirtualCameraResource::scheduleTasksChanged, this,
        &RecordingStatusHelper::updateRecordingMode);

    m_updateTimerId = startTimer(kUpdateInterval.count());

    updateRecordingMode();
}

int RecordingStatusHelper::recordingMode() const
{
    return m_recordingMode;
}

QString RecordingStatusHelper::tooltip() const
{
    return tooltip(m_recordingMode);
}

QString RecordingStatusHelper::shortTooltip() const
{
    return shortTooltip(m_recordingMode);
}

QString RecordingStatusHelper::qmlIconName() const
{
    return qmlIconName(m_recordingMode);
}

QIcon RecordingStatusHelper::icon() const
{
    return icon(m_recordingMode);
}

// TODO: #Elric this should be a resource parameter that is updated from the server.
int RecordingStatusHelper::currentRecordingMode(const QnVirtualCameraResourcePtr& camera)
{
    if (!camera || camera->isScheduleDisabled())
        return Qn::RT_Never;

    const auto dateTime = qnSyncTime->currentDateTime();
    const int dayOfWeek = dateTime.date().dayOfWeek();
    const int seconds = dateTime.time().msecsSinceStartOfDay() / 1000;

    for (const auto& task: camera->getScheduleTasks())
    {
        if (task.getDayOfWeek() == dayOfWeek
            && qBetween(task.getStartTime(), seconds, task.getEndTime() + 1))
        {
            return task.getRecordingType();
        }
    }

    return Qn::RT_Never;
}

QString RecordingStatusHelper::tooltip(int recordingMode)
{
    switch (recordingMode)
    {
        case Qn::RT_Never:
            return tr("Not recording");
        case Qn::RT_Always:
            return tr("Recording everything");
        case Qn::RT_MotionOnly:
            return tr("Recording motion only");
        case Qn::RT_MotionAndLowQuality:
            return tr("Recording motion and low quality");
    }
    return QString();
}

QString RecordingStatusHelper::shortTooltip(int recordingMode)
{
    switch (recordingMode)
    {
        case Qn::RT_Never:
            return tr("Not recording");
        case Qn::RT_Always:
            return tr("Continuous");
        case Qn::RT_MotionOnly:
            return tr("Motion only");
        case Qn::RT_MotionAndLowQuality:
            return tr("Motion + Lo-Res");
    }
    return QString();
}

QString RecordingStatusHelper::qmlIconName(int recordingMode)
{
    switch (recordingMode)
    {
        case Qn::RT_Never:
            return lit("qrc:/skin/item/recording_off.png");
        case Qn::RT_Always:
            return lit("qrc:/skin/item/recording.png");
        case Qn::RT_MotionOnly:
            return lit("qrc:/skin/item/recording_motion.png");
        case Qn::RT_MotionAndLowQuality:
            return lit("qrc:/skin/item/recording_motion_lq.png");
    }
    return QString();
}

QIcon RecordingStatusHelper::icon(int recordingMode)
{
    switch (recordingMode)
    {
        case Qn::RT_Never:
            return qnSkin->icon("item/recording_off.png");
        case Qn::RT_Always:
            return qnSkin->icon("item/recording.png");
        case Qn::RT_MotionOnly:
            return qnSkin->icon("item/recording_motion.png");
        case Qn::RT_MotionAndLowQuality:
            return qnSkin->icon("item/recording_motion_lq.png");
    }
    return QIcon();
}

void RecordingStatusHelper::registerQmlType()
{
    qmlRegisterType<RecordingStatusHelper>("nx.client.desktop", 1, 0, "RecordingStatusHelper");
}

void RecordingStatusHelper::updateRecordingMode()
{
    const auto newMode = currentRecordingMode(m_camera);

    if (newMode == m_recordingMode)
        return;

    m_recordingMode = newMode;
    emit recordingModeChanged();
}

void RecordingStatusHelper::timerEvent(QTimerEvent* event)
{
    if (event->timerId() != m_updateTimerId)
        return;

    updateRecordingMode();
}

} // namespace desktop
} // namespace client
} // namespace nx
