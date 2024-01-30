// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "common_object_search_setup.h"

#include <chrono>

#include <QtCore/QTimer>
#include <QtQml/QtQml>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <utils/common/synctime.h>
#include <nx/utils/log/log.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/core/event_search/models/abstract_search_list_model.h>
#include <nx/vms/client/core/event_search/utils/text_filter_setup.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/utils/managed_camera_set.h>

namespace nx::vms::client::core {

using namespace std::chrono;

namespace {

static constexpr milliseconds kTimeSelectionDelay = 250ms;

} // namespace

class CommonObjectSearchSetup::Private: public QObject
{
    CommonObjectSearchSetup* const q;

public:
    Private(CommonObjectSearchSetup* q);

    AbstractSearchListModel* model() const { return m_model; }
    void setModel(AbstractSearchListModel* value);

    SystemContext* context() const;
    void setContext(SystemContext* value);

    EventSearch::TimeSelection timeSelection() const { return m_timeSelection; }
    void setTimeSelection(EventSearch::TimeSelection value);

    EventSearch::CameraSelection cameraSelection() const { return m_cameraSelection; }
    void setCameraSelection(EventSearch::CameraSelection value);
    void setCustomCameras(const QnVirtualCameraResourceSet& value);
    bool chooseCustomCameras();

    void handleTimelineSelectionChanged(const QnTimePeriod& selection);
    void updateRelevantCamerasForMode(EventSearch::CameraSelection mode);

private:
    void updateRelevantCameras();
    void updateRelevantTimePeriod();
    void setCurrentDate(const QDateTime& value);
    QnTimePeriod interestTimePeriod() const;

private:
    QPointer<AbstractSearchListModel> m_model;
    nx::utils::ScopedConnections m_modelConnections;

    SystemContext* m_context = nullptr;

    EventSearch::TimeSelection m_timeSelection = EventSearch::TimeSelection::anytime;
    EventSearch::TimeSelection m_previousTimeSelection = EventSearch::TimeSelection::anytime;
    QnTimePeriod m_currentTimePeriod;
    QDateTime m_currentDate; //< Either client or server time, depending on the local settings.
    const std::unique_ptr<QTimer> m_dayChangeTimer{new QTimer()};

    QnTimePeriod m_timelineSelection;
    nx::utils::PendingOperation m_applyTimelineSelectionOperation;

    EventSearch::CameraSelection m_cameraSelection = EventSearch::CameraSelection::all;
};

// ------------------------------------------------------------------------------------------------
// CommonObjectSearchSetup

void CommonObjectSearchSetup::registerQmlType()
{
    qmlRegisterUncreatableType<CommonObjectSearchSetup>("nx.vms.client.core", 1, 0,
        "CommonObjectSearchSetup", "Cannot create instance of core CommonObjectSearchSetup");
}

CommonObjectSearchSetup::CommonObjectSearchSetup(QObject* parent):
    QObject(parent),
    d(new Private(this))
{
    connect(this, &CommonObjectSearchSetup::cameraSelectionChanged,
        this, &CommonObjectSearchSetup::selectedCamerasTextChanged);
    connect(this, &CommonObjectSearchSetup::selectedCamerasChanged,
        this, &CommonObjectSearchSetup::selectedCamerasTextChanged);

    connect(this, &CommonObjectSearchSetup::modelChanged,
        this, &CommonObjectSearchSetup::parametersChanged);
    connect(this, &CommonObjectSearchSetup::timeSelectionChanged,
        this, &CommonObjectSearchSetup::parametersChanged);
    connect(this, &CommonObjectSearchSetup::cameraSelectionChanged,
        this, &CommonObjectSearchSetup::parametersChanged);
    connect(this, &CommonObjectSearchSetup::selectedCamerasTextChanged,
        this, &CommonObjectSearchSetup::parametersChanged);
    connect(this, &CommonObjectSearchSetup::selectedCamerasChanged,
        this, &CommonObjectSearchSetup::parametersChanged);
    connect(this, &CommonObjectSearchSetup::singleCameraChanged,
        this, &CommonObjectSearchSetup::parametersChanged);
    connect(this, &CommonObjectSearchSetup::mixedDevicesChanged,
        this, &CommonObjectSearchSetup::parametersChanged);
}

CommonObjectSearchSetup::~CommonObjectSearchSetup()
{
}

SystemContext* CommonObjectSearchSetup::context() const
{
    return d->context();
}

void CommonObjectSearchSetup::setContext(SystemContext* value)
{
    d->setContext(value);
}

AbstractSearchListModel* CommonObjectSearchSetup::model() const
{
    return d->model();
}

void CommonObjectSearchSetup::setModel(AbstractSearchListModel* value)
{
    d->setModel(value);
}

EventSearch::TimeSelection CommonObjectSearchSetup::timeSelection() const
{
    return d->timeSelection();
}

void CommonObjectSearchSetup::setTimeSelection(EventSearch::TimeSelection value)
{
    d->setTimeSelection(value);
}

EventSearch::CameraSelection CommonObjectSearchSetup::cameraSelection() const
{
    return d->cameraSelection();
}

void CommonObjectSearchSetup::setCameraSelection(EventSearch::CameraSelection value)
{
    d->setCameraSelection(value);
}

bool CommonObjectSearchSetup::chooseCustomCameras()
{
    return d->chooseCustomCameras();
}

QnVirtualCameraResourceSet CommonObjectSearchSetup::selectedCameras() const
{
    return model() ? model()->cameraSet().cameras() : QnVirtualCameraResourceSet();
}

void CommonObjectSearchSetup::setSelectedCameras(const QnVirtualCameraResourceSet& value)
{
    d->setCustomCameras(value);
}

QnUuidList CommonObjectSearchSetup::selectedCamerasIds()
{
    QnUuidList result;
    for (const auto& resource: selectedCameras())
        result.push_back(resource->getId());
    return result;
}

void CommonObjectSearchSetup::setSelectedCamerasIds(const QnUuidList& values)
{
    const auto systemContext = context();
    if (!NX_ASSERT(systemContext))
        return;

    const auto source =
        [this]()
    {
        QnUuidSet result;
        for (const auto& resource: selectedCameras())
            result.insert(resource->getId());
        return result;
    }();

    if (source == QnUuidSet(values.begin(), values.end()))
        return; //< No changes in resources.

    setCameraSelection(core::EventSearch::CameraSelection::custom);

    const auto pool = systemContext->resourcePool();
    QnVirtualCameraResourceSet cameras;
    for (const auto& id: values)
    {
        if (const auto resource = pool->getResourceById<QnVirtualCameraResource>(id))
            cameras.insert(resource);
    }

    setSelectedCameras(cameras);
}

TextFilterSetup* CommonObjectSearchSetup::textFilter() const
{
    return model() ? model()->textFilter() : nullptr;
}

QnVirtualCameraResource* CommonObjectSearchSetup::singleCameraRaw() const
{
    const auto camera = singleCamera();
    if (!camera)
        return nullptr;

    QQmlEngine::setObjectOwnership(camera.get(), QQmlEngine::CppOwnership);
    return camera.get();
}

QnVirtualCameraResourcePtr CommonObjectSearchSetup::singleCamera() const
{
    const auto cameras = selectedCameras();
    return cameras.size() == 1
        ? *cameras.constBegin()
        : QnVirtualCameraResourcePtr();
}

int CommonObjectSearchSetup::cameraCount() const
{
    return selectedCameras().size();
}

bool CommonObjectSearchSetup::mixedDevices() const
{
    const auto systemContext = context();
    return systemContext && systemContext->resourcePool()->containsIoModules();
}

void CommonObjectSearchSetup::handleTimelineSelectionChanged(const QnTimePeriod& selection)
{
    d->handleTimelineSelectionChanged(selection);
}

void CommonObjectSearchSetup::updateRelevantCamerasForMode(EventSearch::CameraSelection mode)
{
    d->updateRelevantCamerasForMode(mode);
}

// ------------------------------------------------------------------------------------------------
// CommonObjectSearchSetup::Private

CommonObjectSearchSetup::Private::Private(CommonObjectSearchSetup* q):
    q(q)
{
    m_dayChangeTimer->callOnTimeout(this,
        [this]()
        {
            const auto timeWatcher = m_context
                ? m_context->serverTimeWatcher()
                : nullptr;

            if (timeWatcher)
                setCurrentDate(timeWatcher->displayTime(qnSyncTime->currentMSecsSinceEpoch()));
        });

    m_dayChangeTimer->start(1s);

    m_applyTimelineSelectionOperation.setInterval(kTimeSelectionDelay);
    m_applyTimelineSelectionOperation.setFlags(nx::utils::PendingOperation::FireOnlyWhenIdle);
    m_applyTimelineSelectionOperation.setCallback(
        [this]()
        {
            const bool selectionExists = !m_timelineSelection.isNull();
            const bool selectionFilter = m_timeSelection == EventSearch::TimeSelection::selection;

            if (selectionExists != selectionFilter)
            {
                setTimeSelection(selectionExists
                        ? EventSearch::TimeSelection::selection
                        : m_previousTimeSelection);
            }
            else if (selectionFilter)
            {
                updateRelevantTimePeriod();
            }
        });
}

void CommonObjectSearchSetup::Private::handleTimelineSelectionChanged(
    const QnTimePeriod& selection)
{
    m_timelineSelection = selection;

    // If selection was cleared update immediately, otherwise update after small delay.
    if (m_timelineSelection.isNull())
        m_applyTimelineSelectionOperation.fire();
    else
        m_applyTimelineSelectionOperation.requestOperation();
}

void CommonObjectSearchSetup::Private::updateRelevantCamerasForMode(
    EventSearch::CameraSelection mode)
{
    if (m_cameraSelection == mode)
        updateRelevantCameras();
}

SystemContext* CommonObjectSearchSetup::Private::context() const
{
    return m_context;
}

void CommonObjectSearchSetup::Private::setContext(SystemContext* value)
{
    if (m_context == value)
        return;

    m_context = value;
    emit q->mixedDevicesChanged();

    updateRelevantCameras();
}

void CommonObjectSearchSetup::Private::setModel(AbstractSearchListModel* value)
{
    if (m_model == value)
        return;

    m_modelConnections.reset();
    m_model = value;

    emit q->modelChanged();

    if (!m_model)
        return;

    m_modelConnections << connect(m_model.data(), &AbstractSearchListModel::camerasChanged,
        q, &CommonObjectSearchSetup::selectedCamerasChanged);

    m_modelConnections << connect(m_model.data(), &AbstractSearchListModel::isOnlineChanged, this,
        [this](bool isOnline)
        {
            m_model->cameraSet().invalidateFilter();
            emit q->mixedDevicesChanged();

            if (isOnline)
                updateRelevantCameras();
        });

    updateRelevantTimePeriod();
    updateRelevantCameras();
}

void CommonObjectSearchSetup::Private::setCameraSelection(EventSearch::CameraSelection value)
{
    if (m_cameraSelection == value)
        return;

    m_cameraSelection = value;
    updateRelevantCameras();
    emit q->cameraSelectionChanged();
    emit q->selectedCamerasChanged();
}

void CommonObjectSearchSetup::Private::setTimeSelection(EventSearch::TimeSelection value)
{
    if (m_timeSelection == value)
        return;

    m_previousTimeSelection = m_timeSelection;
    m_timeSelection = value;

    if (m_timeSelection != EventSearch::TimeSelection::selection)
        q->clearTimelineSelection();

    updateRelevantTimePeriod();
    emit q->timeSelectionChanged();
}

void CommonObjectSearchSetup::Private::updateRelevantCameras()
{
    if (!m_model)
        return;

    switch (m_cameraSelection)
    {
        case EventSearch::CameraSelection::all:
            m_model->cameraSet().setAllCameras();
            break;
        case EventSearch::CameraSelection::layout:
            m_model->cameraSet().setMultipleCameras(q->currentLayoutCameras());
            break;
        case EventSearch::CameraSelection::current:
            m_model->cameraSet().setSingleCamera(q->currentResource());
            break;
        case EventSearch::CameraSelection::custom:
            break; //< Custom cameras are selected in chooseCustomCameras()
        default:
            NX_ASSERT(false);
            break;
    }
}

bool CommonObjectSearchSetup::Private::chooseCustomCameras()
{
    if (!NX_ASSERT(m_context && m_model))
        return false;

    QnUuidSet chosenIds;
    if (m_cameraSelection == EventSearch::CameraSelection::custom)
    {
        for (const auto& camera: m_model->cameraSet().cameras())
            chosenIds.insert(camera->getId());
    }

    if (!q->selectCameras(chosenIds))
    {
        return false; //< Cancel, if user cancelled the selection.
    }

    const auto resources =
        m_context->resourcePool()->getResourcesByIds<QnVirtualCameraResource>(chosenIds);
    const auto cameras = QSet(resources.cbegin(), resources.cend());
    if (cameras.empty())
        return false; //< Cancel, if user didn't select any camera.

    if (m_cameraSelection == EventSearch::CameraSelection::custom
        && cameras == m_model->cameraSet().cameras())
    {
        return false; //< Cancel, if user selected the same camera(s) as before.
    }

    setCustomCameras(cameras);
    return true;
}

void CommonObjectSearchSetup::Private::setCustomCameras(const QnVirtualCameraResourceSet& value)
{
    if (!NX_ASSERT(m_model))
        return;

    setCameraSelection(EventSearch::CameraSelection::custom);
    m_model->cameraSet().setMultipleCameras(value);
}

void CommonObjectSearchSetup::Private::updateRelevantTimePeriod()
{
    if (!m_model)
        return;

    const auto newTimePeriod = interestTimePeriod();
    if (m_currentTimePeriod == newTimePeriod)
        return;

    m_currentTimePeriod = newTimePeriod;

    m_model->setInterestTimePeriod(m_currentTimePeriod);
}

void CommonObjectSearchSetup::Private::setCurrentDate(const QDateTime& value)
{
    auto startOfDay = value;
    startOfDay.setTime(QTime(0, 0));

    if (m_currentDate == startOfDay)
        return;

    m_currentDate = startOfDay;
    updateRelevantTimePeriod();
}

QnTimePeriod CommonObjectSearchSetup::Private::interestTimePeriod() const
{
    int days = 0;
    switch (m_timeSelection)
    {
        case EventSearch::TimeSelection::anytime:
            return QnTimePeriod::anytime();

        case EventSearch::TimeSelection::selection:
            return m_timelineSelection;

        case EventSearch::TimeSelection::day:
            days = 1;
            break;

        case EventSearch::TimeSelection::week:
            days = 7;
            break;

        case EventSearch::TimeSelection::month:
            days = 30;
            break;

        default:
            NX_ASSERT(false);
            break;
    }

    return QnTimePeriod(m_currentDate.addDays(1 - days).toMSecsSinceEpoch(),
        QnTimePeriod::kInfiniteDuration);
}

} // namespace nx::vms::client::core
