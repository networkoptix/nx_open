// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "caching_camera_data_loader.h"

#include <QtCore/QMetaType>
#include <QtCore/QTimer>

#include <camera/loaders/flat_camera_data_loader.h>
#include <camera/loaders/layout_file_camera_data_loader.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/datetime.h>
#include <nx/utils/log/log.h>
#include <utils/common/synctime.h>

namespace {

const qint64 requestIntervalMs = 30 * 1000;

} // namespace

using AllowedContent = QnCachingCameraDataLoader::AllowedContent;
using StorageLocation = nx::vms::api::StorageLocation;

QnCachingCameraDataLoader::QnCachingCameraDataLoader(
    const QnMediaResourcePtr &resource,
    QObject* parent)
    :
    base_type(parent),
    m_resource(resource)
{
    NX_ASSERT(supportedResource(resource),
        "Loaders must not be created for unsupported resources");
    init();
    initLoaders();

    m_analyticsFilter.deviceIds.insert(m_resource->toResource()->getId());

    QTimer* loadTimer = new QTimer(this);
    // Time period will be loaded no often than once in 30 seconds,
    // but timer should check it much more often.
    loadTimer->setInterval(requestIntervalMs / 10);
    loadTimer->setSingleShot(false);
    connect(loadTimer, &QTimer::timeout, this,
        [this]
        {
            NX_VERBOSE(this, "Checking load by timer (allowed: %1)", m_allowedContent);
            load();
        });
    loadTimer->start();
    load(true);
}

QnCachingCameraDataLoader::~QnCachingCameraDataLoader() {
}

bool QnCachingCameraDataLoader::supportedResource(const QnMediaResourcePtr &resource) {
    bool result = !resource.dynamicCast<QnVirtualCameraResource>().isNull();
    result |= !resource.dynamicCast<QnAviResource>().isNull();
    return result;
}

void QnCachingCameraDataLoader::init() {
    // TODO: #sivanov Move to camera history.
    if (m_resource.dynamicCast<QnNetworkResource>())
    {
        connect(qnSyncTime, &QnSyncTime::timeChanged,
                this, &QnCachingCameraDataLoader::discardCachedData);
    }
}

void QnCachingCameraDataLoader::initLoaders() {
    QnVirtualCameraResourcePtr camera = m_resource.dynamicCast<QnVirtualCameraResource>();
    QnAviResourcePtr aviFile = m_resource.dynamicCast<QnAviResource>();

    for(int i = 0; i < Qn::TimePeriodContentCount; i++) {
        Qn::TimePeriodContent dataType = static_cast<Qn::TimePeriodContent>(i);
        QnAbstractCameraDataLoader* loader = nullptr;

        if (camera)
            loader = new QnFlatCameraDataLoader(camera, dataType);
        else if (aviFile)
            loader = new QnLayoutFileCameraDataLoader(aviFile, dataType);

        m_loaders[i].reset(loader);

        if (loader)
        {
            connect(loader, &QnAbstractCameraDataLoader::ready, this,
                [this, dataType]
                (qint64 startTimeMs)
                {
                    NX_VERBOSE(this, "Handling %1 reply for period %2 (%3)",
                        dataType,
                        startTimeMs,
                        nx::utils::timestampToDebugString(startTimeMs));

                    emit periodsChanged(dataType, startTimeMs);
                });

            connect(loader, &QnAbstractCameraDataLoader::failed, this,
                [this, dataType]()
                {
                    NX_VERBOSE(this, "Chunks %1: failed to load", dataType);
                    emit loadingFailed();
                });
        }
    }
}

void QnCachingCameraDataLoader::setAllowedContent(AllowedContent value)
{
    if (m_allowedContent == value)
        return;

    NX_VERBOSE(this, "Set loader allowed content to %1", value);

    AllowedContent addedContent;
    std::set_difference(value.cbegin(), value.cend(),
        m_allowedContent.cbegin(), m_allowedContent.cend(),
        std::inserter(addedContent, addedContent.begin()));

    m_allowedContent = value;

    for (auto contentType: addedContent)
        updateTimePeriods(contentType, /*forced*/ true);
}

AllowedContent QnCachingCameraDataLoader::allowedContent() const
{
    return m_allowedContent;
}

bool QnCachingCameraDataLoader::isContentAllowed(Qn::TimePeriodContent content) const
{
    return m_allowedContent.find(content) != m_allowedContent.cend();
}

QString QnCachingCameraDataLoader::idForToStringFromPtr() const
{
    return m_resource->toResourcePtr()->getName();
}

QnMediaResourcePtr QnCachingCameraDataLoader::resource() const
{
    return m_resource;
}

void QnCachingCameraDataLoader::load(bool forced)
{
    for (auto content: m_allowedContent)
        updateTimePeriods(content, forced);
}

void QnCachingCameraDataLoader::setMotionSelection(const MotionSelection& motionRegions)
{
    if (m_motionSelection == motionRegions)
        return;

    m_motionSelection = motionRegions;
    discardCachedDataType(Qn::MotionContent);
}

const QnCachingCameraDataLoader::AnalyticsFilter& QnCachingCameraDataLoader::analyticsFilter() const
{
    return m_analyticsFilter;
}

void QnCachingCameraDataLoader::setAnalyticsFilter(const AnalyticsFilter& value)
{
    if (m_analyticsFilter == value)
        return;

    const auto deviceId = m_analyticsFilter.deviceIds.empty()
        ? QnUuid()
        : *m_analyticsFilter.deviceIds.cbegin();

    using DeviceIds = decltype(value.deviceIds);
    NX_ASSERT(value.deviceIds == DeviceIds{deviceId});

    m_analyticsFilter = value;

    if (m_analyticsFilter.deviceIds != DeviceIds{deviceId})
        m_analyticsFilter.deviceIds = DeviceIds{deviceId}; //< Just for safety.

    discardCachedDataType(Qn::AnalyticsContent);
}

void QnCachingCameraDataLoader::setStorageLocation(StorageLocation value)
{
    if (m_storageLocation == value)
        return;

    NX_VERBOSE(this, "Select storage location: %1", value);
    m_storageLocation = value;

    for (int type = 0; type < Qn::TimePeriodContentCount; type++)
    {
        if (auto loader = m_loaders[type])
            loader->setStorageLocation(value);
    }
    discardCachedData();
}

const QnTimePeriodList& QnCachingCameraDataLoader::periods(
    Qn::TimePeriodContent timePeriodType) const
{
    if (auto loader = m_loaders[timePeriodType])
        return loader->periods();

    static QnTimePeriodList kEmptyPeriods;
    return kEmptyPeriods;
}

bool QnCachingCameraDataLoader::loadInternal(Qn::TimePeriodContent periodType)
{
    QnAbstractCameraDataLoaderPtr loader = m_loaders[periodType];
    NX_ASSERT(loader, "Loader must always exists");

    if (!loader)
    {
        emit loadingFailed();
        return false;
    }

    switch (periodType)
    {
        case Qn::RecordingContent:
        {
            loader->load();
            return true;
        }

        case Qn::MotionContent:
        {
            const bool isMotionSelectionEmpty = std::all_of(
                m_motionSelection.cbegin(),
                m_motionSelection.cend(),
                [](const QRegion& region) { return region.isEmpty(); });

            const auto filter = isMotionSelectionEmpty
                ? QString() //< Treat empty selection as whole area selection.
                : QJson::serialized(m_motionSelection);

            loader->load(filter);
            return true;
        }

        case Qn::AnalyticsContent:
        {
            // Minimum required value to provide continuous display on the timeline.
            const qint64 analyticsDetailLevelMs = 1000;
            const auto filter = QString::fromUtf8(QJson::serialized(m_analyticsFilter));
            loader->load(filter, analyticsDetailLevelMs);
            return true;
        }

        default:
        {
            NX_ASSERT(false, "Should never get here");
            break;
        }
    }
    return false;
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnCachingCameraDataLoader::invalidateCachedData()
{
    NX_VERBOSE(this, "Mark local cache as dirty");

    for (auto loader: m_loaders)
        loader->discardCachedData();
}

void QnCachingCameraDataLoader::discardCachedDataType(Qn::TimePeriodContent type)
{
    NX_VERBOSE(this, "Discard cached %1 chunks", type);
    if (auto loader = m_loaders[type])
        loader->discardCachedData();

    if (isContentAllowed(type))
    {
        updateTimePeriods(type, true);
        emit periodsChanged(type);
    }
}

void QnCachingCameraDataLoader::discardCachedData()
{
    NX_VERBOSE(this, "Discard all cached data");
    for (int i = 0; i < Qn::TimePeriodContentCount; i++)
        discardCachedDataType(Qn::TimePeriodContent(i));
}

void QnCachingCameraDataLoader::updateTimePeriods(Qn::TimePeriodContent periodType, bool forced) {
    // TODO: #sivanov Make sure we are not sending requests while loader is disabled.
    if (forced || m_previousRequestTime[periodType].hasExpired(requestIntervalMs))
    {
        if (forced)
            NX_VERBOSE(this, "updateTimePeriods %1 (forced)", periodType);
        else
            NX_VERBOSE(this, "updateTimePeriods %1 (by timer)", periodType);

        if (loadInternal(periodType))
            m_previousRequestTime[periodType].restart();
    }
}
