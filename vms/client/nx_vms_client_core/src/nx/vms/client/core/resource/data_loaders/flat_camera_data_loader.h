// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtGui/QRegion>

#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/resource_fwd.h>
#include <recording/time_period.h>
#include <recording/time_period_list.h>

#include "abstract_camera_data_loader.h"

namespace nx::vms::client::core {

/**
 * Per-camera data loader that caches loaded data.
 * Uses flat structure. Data is loaded with the most detailed level.
 * Source data period is solid, no spaces are allowed.
 */
class FlatCameraDataLoader: public AbstractCameraDataLoader
{
    Q_OBJECT
public:
    /**
     * Constructor.
     *
     * \param resource                  Network resource representing the camera to work with.
     * \param parent                    Parent object.
     */
    FlatCameraDataLoader(const QnVirtualCameraResourcePtr& camera,
        Qn::TimePeriodContent dataType,
        QObject* parent = nullptr);
    virtual ~FlatCameraDataLoader();

    virtual void load(const QString &filter = QString(), const qint64 resolutionMs = 1) override;

    virtual void discardCachedData(const qint64 resolutionMs = 0) override;

    /** Limit chunks loading to only main or backup storage - or allow both (default). */
    virtual void setStorageLocation(nx::vms::api::StorageLocation value) override;

private:
    void at_timePeriodsReceived(bool success,
        int requestHandle,
        const MultiServerPeriodDataList &timePeriods);

private:
    int sendRequest(qint64 startTimeMs, qint64 resolutionMs);
    void handleDataLoaded(bool success, QnTimePeriodList&& periods, int requestHandle);

private:
    struct LoadingInfo
    {
        /** Starting time of the request. */
        qint64 startTimeMs = 0;
    };

    QString m_filter;
    nx::vms::api::StorageLocation m_storageLocation = nx::vms::api::StorageLocation::both;

    LoadingInfo m_loading;
};

} // namespace nx::vms::client::core
