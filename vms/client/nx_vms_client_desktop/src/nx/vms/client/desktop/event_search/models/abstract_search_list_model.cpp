#include "abstract_search_list_model.h"

#include <chrono>

#include <client/client_message_processor.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <ui/workbench/workbench_access_controller.h>

#include <nx/vms/client/desktop/utils/managed_camera_set.h>
#include <nx/utils/datetime.h>
#include <nx/utils/log/log.h>

namespace nx::vms::client::desktop {

AbstractSearchListModel::AbstractSearchListModel(QnWorkbenchContext* context, QObject* parent):
    base_type(context, parent),
    m_cameraSet(new ManagedCameraSet(resourcePool(),
        [this](const QnVirtualCameraResourcePtr& camera)
        {
            return isCameraApplicable(camera);
        }))
{
    NX_CRITICAL(commonModule() && commonModule()->messageProcessor() && resourcePool());

    const auto updateIsOnline =
        [this]()
        {
            const bool isOnline =
                qnClientMessageProcessor->connectionStatus()->state() == QnConnectionState::Ready;

            if (m_isOnline == isOnline)
                return;

            m_isOnline = isOnline;

            if (m_isOnline)
            {
                emit dataNeeded();
            }
            else
            {
                m_cameraSet->setMultipleCameras({});
                clear();
            }

            emit isOnlineChanged(m_isOnline, {});
        };

    updateIsOnline();

    connect(qnClientMessageProcessor->connectionStatus(), &QnClientConnectionStatus::stateChanged,
        this, updateIsOnline);

    connect(m_cameraSet.data(), &ManagedCameraSet::camerasAboutToBeChanged, this,
        [this]()
        {
            emit camerasAboutToBeChanged({});
            clear();
        });

    connect(m_cameraSet.data(), &ManagedCameraSet::camerasChanged, this,
        [this]()
        {
            NX_ASSERT(isOnline() || m_cameraSet->cameras().isEmpty());
            emit camerasChanged({});
        });
}

AbstractSearchListModel::~AbstractSearchListModel()
{
    // Required here for forward-declared scoped pointer destruction.
}

bool AbstractSearchListModel::canFetchMore(const QModelIndex& parent) const
{
    return parent.isValid() || !isOnline() || (isLive() && fetchDirection() == FetchDirection::later)
        ? false
        : canFetch();
}

void AbstractSearchListModel::fetchMore(const QModelIndex& parent)
{
    if (!parent.isValid())
        requestFetch();
}

ManagedCameraSet* AbstractSearchListModel::cameraSet() const
{
    return m_cameraSet.data();
}

bool AbstractSearchListModel::isOnline() const
{
    return m_isOnline;
}

QnVirtualCameraResourceSet AbstractSearchListModel::cameras() const
{
    return m_cameraSet->cameras();
}

bool AbstractSearchListModel::fetchInProgress() const
{
    return false;
}

bool AbstractSearchListModel::cancelFetch()
{
    return false;
}

bool AbstractSearchListModel::isConstrained() const
{
    return m_relevantTimePeriod != QnTimePeriod::anytime();
}

const QnTimePeriod& AbstractSearchListModel::relevantTimePeriod() const
{
    return m_relevantTimePeriod;
}

void AbstractSearchListModel::setRelevantTimePeriod(const QnTimePeriod& value)
{
    if (value == m_relevantTimePeriod)
        return;

    NX_VERBOSE(this) << "Relevant time period changed"
        << "\n --- Old was from"
        << utils::timestampToDebugString(m_relevantTimePeriod.startTimeMs) << "to"
        << utils::timestampToDebugString(m_relevantTimePeriod.endTimeMs())
        << "\n --- New is from"
        << utils::timestampToDebugString(value.startTimeMs) << "to"
        << utils::timestampToDebugString(value.endTimeMs());

    m_relevantTimePeriod = value;

    if (!m_relevantTimePeriod.isValid() || !m_fetchedTimeWindow.contains(m_relevantTimePeriod))
    {
        clear();
    }
    else
    {
        cancelFetch();
        truncateToRelevantTimePeriod();
        setFetchedTimeWindow(m_relevantTimePeriod);

        if (!m_relevantTimePeriod.isInfinite())
            setLive(false);

        if (canFetchMore())
            emit dataNeeded();
    }
}

AbstractSearchListModel::FetchDirection AbstractSearchListModel::fetchDirection() const
{
    return m_fetchDirection;
}

void AbstractSearchListModel::setFetchDirection(FetchDirection value)
{
    if (m_fetchDirection == value)
        return;

    NX_VERBOSE(this) << "Set fetch direction to" << value;

    cancelFetch();
    m_fetchDirection = value;
}

int AbstractSearchListModel::maximumCount() const
{
    return m_maximumCount;
}

void AbstractSearchListModel::setMaximumCount(int value)
{
    if (m_maximumCount == value)
        return;

    NX_VERBOSE(this) << "Set maximum count to" << value;

    m_maximumCount = value;

    const bool needToTruncate = m_maximumCount < rowCount();
    if (needToTruncate)
    {
        NX_VERBOSE(this) << "Truncating to maximum count";
        truncateToMaximumCount();
    }
}

int AbstractSearchListModel::fetchBatchSize() const
{
    return m_fetchBatchSize;
}

void AbstractSearchListModel::setFetchBatchSize(int value)
{
    NX_VERBOSE(this) << "Set fetch batch size to" << value;
    m_fetchBatchSize = value;
}

bool AbstractSearchListModel::liveSupported() const
{
    return m_liveSupported;
}

bool AbstractSearchListModel::effectiveLiveSupported() const
{
    return liveSupported() && relevantTimePeriod().isInfinite();
}

bool AbstractSearchListModel::isLive() const
{
    return m_liveSupported && m_live;
}

void AbstractSearchListModel::setLive(bool value)
{
    if (value == m_live)
        return;

    if (value)
    {
        const bool supported = effectiveLiveSupported();
        NX_ASSERT(supported);
        if (!supported)
            return;
    }

    NX_VERBOSE(this) << "Setting live mode to" << value;
    m_live = value;

    emit liveChanged(m_live, {});
}

bool AbstractSearchListModel::livePaused() const
{
    return m_livePaused;
}

void AbstractSearchListModel::setLivePaused(bool value)
{
    if (m_livePaused == value)
        return;

    NX_VERBOSE(this) << "Setting live" << (value ? "paused" : "unpaused");
    m_livePaused = value;

    emit livePausedChanged(m_livePaused, {});
}

void AbstractSearchListModel::setLiveSupported(bool value)
{
    if (m_liveSupported == value)
        return;

    m_liveSupported = value;
    setLive(m_live && m_liveSupported);

    clear();
}

QnTimePeriod AbstractSearchListModel::fetchedTimeWindow() const
{
    return m_fetchedTimeWindow;
}

bool AbstractSearchListModel::isCameraApplicable(const QnVirtualCameraResourcePtr& camera) const
{
    return camera && !camera->flags().testFlag(Qn::desktop_camera)
        && accessController()->hasPermissions(camera,
            Qn::ReadPermission | Qn::ViewContentPermission);
}

void AbstractSearchListModel::setFetchedTimeWindow(const QnTimePeriod& value)
{
    NX_VERBOSE(this) << "Set fetched time window from"
        << utils::timestampToDebugString(value.startTimeMs) << "to"
        << utils::timestampToDebugString(value.endTimeMs());

    m_fetchedTimeWindow = value;
}

void AbstractSearchListModel::addToFetchedTimeWindow(const QnTimePeriod& period)
{
    auto timeWindow = m_fetchedTimeWindow;
    timeWindow.addPeriod(period);
    setFetchedTimeWindow(timeWindow);
}

void AbstractSearchListModel::finishFetch(FetchResult result)
{
    NX_VERBOSE(this) << "Finish fetch;" << QVariant::fromValue(result).toString();
    emit fetchFinished(result, {});
}

void AbstractSearchListModel::clear()
{
    NX_VERBOSE(this) << "Clear model";

    setFetchDirection(FetchDirection::earlier);
    setFetchedTimeWindow({});
    cancelFetch();
    clearData();

    setLive(effectiveLiveSupported());
    if (canFetchMore())
        emit dataNeeded();
}

} // namespace nx::vms::client::desktop
