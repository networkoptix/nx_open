// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtGui/QRegion>

#include <api/server_rest_connection_fwd.h>
#include <camera/loaders/abstract_camera_data_loader.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <recording/time_period.h>
#include <recording/time_period_list.h>

/**
 * Per-camera data loader that caches loaded data.
 * Uses flat structure. Data is loaded with the most detailed level.
 * Source data period is solid, no spaces are allowed.
 */
class QnFlatCameraDataLoader:
    public QnAbstractCameraDataLoader,
    public nx::vms::client::core::RemoteConnectionAware
{
    Q_OBJECT
public:
    /**
     * Constructor.
     *
     * \param resource                  Network resource representing the camera to work with.
     * \param parent                    Parent object.
     */
    QnFlatCameraDataLoader(const QnVirtualCameraResourcePtr& camera,
        Qn::TimePeriodContent dataType,
        QObject* parent = nullptr);
    virtual ~QnFlatCameraDataLoader();

    virtual void load(const QString &filter = QString(), const qint64 resolutionMs = 1) override;

    virtual void discardCachedData(const qint64 resolutionMs = 0) override;

private:
    void at_timePeriodsReceived(bool success,
        rest::Handle requestHandle,
        const MultiServerPeriodDataList &timePeriods);

private:
    rest::Handle sendRequest(qint64 startTimeMs, qint64 resolutionMs);
    void handleDataLoaded(bool success, QnTimePeriodList&& periods);

private:
    struct LoadingInfo
    {
        /** Starting time of the request. */
        qint64 startTimeMs = 0;

        /** Handle of the last request. */
        rest::Handle handle = 0;
    };

    QString m_filter;

    LoadingInfo m_loading;
};
