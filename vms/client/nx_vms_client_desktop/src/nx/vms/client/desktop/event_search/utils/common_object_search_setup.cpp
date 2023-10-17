// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "common_object_search_setup.h"

#include <chrono>

#include <QtCore/QTimer>
#include <QtQml/QtQml>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/log.h>
#include <nx/utils/pending_operation.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/event_search/models/abstract_search_list_model.h>
#include <nx/vms/client/desktop/event_search/utils/text_filter_setup.h>
#include <nx/vms/client/desktop/resource_dialogs/camera_selection_dialog.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/managed_camera_set.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::desktop {

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

    WindowContext* context() const { return m_context; }
    void setContext(WindowContext* value);

    RightPanel::TimeSelection timeSelection() const { return m_timeSelection; }
    void setTimeSelection(RightPanel::TimeSelection value);

    RightPanel::CameraSelection cameraSelection() const { return m_cameraSelection; }
    void setCameraSelection(RightPanel::CameraSelection value);
    void setCustomCameras(const QnVirtualCameraResourceSet& value);
    bool chooseCustomCameras();

private:
    void updateRelevantCameras();
    void updateRelevantTimePeriod();
    void setCurrentDate(const QDateTime& value);
    QnTimePeriod effectiveTimePeriod() const;

private:
    QPointer<AbstractSearchListModel> m_model;
    nx::utils::ScopedConnections m_modelConnections;

    WindowContext* m_context = nullptr;
    nx::utils::ScopedConnections m_contextConnections;

    RightPanel::TimeSelection m_timeSelection = RightPanel::TimeSelection::anytime;
    RightPanel::TimeSelection m_previousTimeSelection = RightPanel::TimeSelection::anytime;
    QnTimePeriod m_currentTimePeriod;
    QDateTime m_currentDate; //< Either client or server time, depending on the local settings.
    const std::unique_ptr<QTimer> m_dayChangeTimer{new QTimer()};

    QnTimePeriod m_timelineSelection;
    nx::utils::PendingOperation m_applyTimelineSelectionOperation;

    RightPanel::CameraSelection m_cameraSelection = RightPanel::CameraSelection::all;
};

// ------------------------------------------------------------------------------------------------
// CommonObjectSearchSetup

CommonObjectSearchSetup::CommonObjectSearchSetup(QObject* parent):
    QObject(parent),
    d(new Private(this))
{
}

CommonObjectSearchSetup::~CommonObjectSearchSetup()
{
    // Required here for forward-declared pointer destruction.
}

AbstractSearchListModel* CommonObjectSearchSetup::model() const
{
    return d->model();
}

void CommonObjectSearchSetup::setModel(AbstractSearchListModel* value)
{
    d->setModel(value);
}

WindowContext* CommonObjectSearchSetup::context() const
{
    return d->context();
}

void CommonObjectSearchSetup::setContext(WindowContext* value)
{
    d->setContext(value);
}

RightPanel::TimeSelection CommonObjectSearchSetup::timeSelection() const
{
    return d->timeSelection();
}

void CommonObjectSearchSetup::setTimeSelection(RightPanel::TimeSelection value)
{
    d->setTimeSelection(value);
}

RightPanel::CameraSelection CommonObjectSearchSetup::cameraSelection() const
{
    return d->cameraSelection();
}

void CommonObjectSearchSetup::setCameraSelection(RightPanel::CameraSelection value)
{
    d->setCameraSelection(value);
}

void CommonObjectSearchSetup::setCustomCameras(const QnVirtualCameraResourceSet& value)
{
    d->setCustomCameras(value);
}

bool CommonObjectSearchSetup::chooseCustomCameras()
{
    return d->chooseCustomCameras();
}

TextFilterSetup* CommonObjectSearchSetup::textFilter() const
{
    return model() ? model()->textFilter() : nullptr;
}

QnVirtualCameraResourceSet CommonObjectSearchSetup::selectedCameras() const
{
    return model() ? model()->cameras() : QnVirtualCameraResourceSet();
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
    return context() && context()->system()->resourcePool()->containsIoModules();
}

// ------------------------------------------------------------------------------------------------
// CommonObjectSearchSetup::Private

CommonObjectSearchSetup::Private::Private(CommonObjectSearchSetup* q):
    q(q)
{
    m_dayChangeTimer->callOnTimeout(this,
        [this]()
        {
            if (!context())
                return;

            const auto timeWatcher = context()->system()->serverTimeWatcher();
            setCurrentDate(timeWatcher->displayTime(qnSyncTime->currentMSecsSinceEpoch()));
        });

    m_dayChangeTimer->start(1s);

    m_applyTimelineSelectionOperation.setInterval(kTimeSelectionDelay);
    m_applyTimelineSelectionOperation.setFlags(nx::utils::PendingOperation::FireOnlyWhenIdle);
    m_applyTimelineSelectionOperation.setCallback(
        [this]()
        {
            const bool selectionExists = !m_timelineSelection.isNull();
            const bool selectionFilter = m_timeSelection == RightPanel::TimeSelection::selection;

            if (selectionExists != selectionFilter)
            {
                setTimeSelection(selectionExists
                    ? RightPanel::TimeSelection::selection
                    : m_previousTimeSelection);
            }
            else if (selectionFilter)
            {
                updateRelevantTimePeriod();
            }
        });
}

void CommonObjectSearchSetup::Private::setContext(WindowContext* value)
{
    if (m_context == value)
        return;

    m_contextConnections.reset();

    m_context = value;
    emit q->contextChanged();
    emit q->mixedDevicesChanged();

    if (!m_context)
        return;

    m_contextConnections << connect(
        m_context->workbenchContext()->navigator(),
        &QnWorkbenchNavigator::timeSelectionChanged,
        this,
        [this](const QnTimePeriod& selection)
        {
            m_timelineSelection = selection;

            // If selection was cleared update immediately, otherwise update after small delay.
            if (m_timelineSelection.isNull())
                m_applyTimelineSelectionOperation.fire();
            else
                m_applyTimelineSelectionOperation.requestOperation();
        });

    const auto camerasUpdaterFor =
        [this](RightPanel::CameraSelection mode)
        {
            const auto updater =
                [this, mode]()
                {
                    if (m_cameraSelection == mode)
                        updateRelevantCameras();
                };

            return updater;
        };

    updateRelevantCameras();

    m_contextConnections << connect(
        m_context->workbenchContext()->navigator(),
        &QnWorkbenchNavigator::currentResourceChanged,
        this,
        camerasUpdaterFor(RightPanel::CameraSelection::current));

    m_contextConnections << connect(m_context->workbench(), &Workbench::currentLayoutChanged,
        this, camerasUpdaterFor(RightPanel::CameraSelection::layout));

    m_contextConnections << connect(m_context->workbench(), &Workbench::currentLayoutItemsChanged,
        this, camerasUpdaterFor(RightPanel::CameraSelection::layout));
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
            m_model->cameraSet()->invalidateFilter();
            emit q->mixedDevicesChanged();

            if (isOnline)
                updateRelevantCameras();
        });

    updateRelevantTimePeriod();
    updateRelevantCameras();
}

void CommonObjectSearchSetup::Private::setCameraSelection(RightPanel::CameraSelection value)
{
    if (m_cameraSelection == value)
        return;

    m_cameraSelection = value;
    updateRelevantCameras();
    emit q->cameraSelectionChanged();
    emit q->selectedCamerasChanged();
}

void CommonObjectSearchSetup::Private::setTimeSelection(RightPanel::TimeSelection value)
{
    if (m_timeSelection == value)
        return;

    m_previousTimeSelection = m_timeSelection;
    m_timeSelection = value;

    if (m_context && m_timeSelection != RightPanel::TimeSelection::selection)
        m_context->workbenchContext()->navigator()->clearTimelineSelection();

    updateRelevantTimePeriod();
    emit q->timeSelectionChanged();
}

void CommonObjectSearchSetup::Private::updateRelevantCameras()
{
    if (!m_model || !m_context)
        return;

    switch (m_cameraSelection)
    {
        case RightPanel::CameraSelection::all:
            m_model->cameraSet()->setAllCameras();
            break;

        case RightPanel::CameraSelection::layout:
        {
            QnVirtualCameraResourceSet cameras;
            if (auto workbenchLayout = m_context->workbench()->currentLayout())
            {
                for (const auto& item: workbenchLayout->items())
                {
                    if (const auto camera = item->resource().dynamicCast<QnVirtualCameraResource>())
                        cameras.insert(camera);
                }
            }

            m_model->cameraSet()->setMultipleCameras(cameras);
            break;
        }

        case RightPanel::CameraSelection::current:
            m_model->cameraSet()->setSingleCamera(
                m_context->workbenchContext()->navigator()->currentResource()
                    .dynamicCast<QnVirtualCameraResource>());
            break;

        case RightPanel::CameraSelection::custom:
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
    if (m_cameraSelection == RightPanel::CameraSelection::custom)
    {
        for (const auto& camera: m_model->cameras())
            chosenIds.insert(camera->getId());
    }

    if (!CameraSelectionDialog::selectCameras<CameraSelectionDialog::DummyPolicy>(
        chosenIds, m_context->mainWindowWidget()))
    {
        return false; //< Cancel, if user cancelled the selection.
    }

    const auto cameras = nx::utils::toQSet(
        m_context->system()->resourcePool()
            ->getResourcesByIds<QnVirtualCameraResource>(chosenIds));

    if (cameras.empty())
        return false; //< Cancel, if user didn't select any camera.

    if (m_cameraSelection == RightPanel::CameraSelection::custom && cameras == m_model->cameras())
        return false; //< Cancel, if user selected the same camera(s) as before.

    setCustomCameras(cameras);
    return true;
}

void CommonObjectSearchSetup::Private::setCustomCameras(const QnVirtualCameraResourceSet& value)
{
    if (!NX_ASSERT(m_context && m_model))
        return;

    setCameraSelection(RightPanel::CameraSelection::custom);
    m_model->cameraSet()->setMultipleCameras(value);
}

void CommonObjectSearchSetup::Private::updateRelevantTimePeriod()
{
    if (!m_model)
        return;

    const auto newTimePeriod = effectiveTimePeriod();
    if (m_currentTimePeriod == newTimePeriod)
        return;

    m_currentTimePeriod = newTimePeriod;
    m_model->setRelevantTimePeriod(m_currentTimePeriod);
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

QnTimePeriod CommonObjectSearchSetup::Private::effectiveTimePeriod() const
{
    int days = 0;
    switch (m_timeSelection)
    {
        case RightPanel::TimeSelection::anytime:
            return QnTimePeriod::anytime();

        case RightPanel::TimeSelection::selection:
            return m_timelineSelection;

        case RightPanel::TimeSelection::day:
            days = 1;
            break;

        case RightPanel::TimeSelection::week:
            days = 7;
            break;

        case RightPanel::TimeSelection::month:
            days = 30;
            break;

        default:
            NX_ASSERT(false);
            break;
    }

    return QnTimePeriod(m_currentDate.addDays(1 - days).toMSecsSinceEpoch(),
        QnTimePeriod::kInfiniteDuration);
}

} // namespace nx::vms::client::desktop
