// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/api/types/storage_location.h>
#include <recording/time_period_list.h>

#include "camera_data_loader_fwd.h"

namespace nx::vms::client::core {

/** Base class for loading custom camera archive-related data. */
class AbstractCameraDataLoader: public QObject
{
    Q_OBJECT

public:
    AbstractCameraDataLoader(
        const QnResourcePtr& resource,
        const Qn::TimePeriodContent dataType,
        QObject* parent);
    virtual ~AbstractCameraDataLoader();

    /**
     * @param filter Custom data filter.
     * @param resolutionMs Minimal length of the data period that should be loaded, in
     *     milliseconds.
     */
    virtual void load(const QString& filter = QString(), const qint64 resolutionMs = 1) = 0;

    /**
     * @returns Resource that this loader works with (camera or archive).
     */
    QnResourcePtr resource() const;

    /**
     * Loaded time periods.
     */
    const QnTimePeriodList& periods() const;

    /**
     * Discards cached data, if any.
     *
     * @param resolutionMs Resolution, data for which should be discarded. All data should be
     *     discarded if parameter equals to 0 (default).
     */
    Q_SLOT virtual void discardCachedData(const qint64 resolutionMs = 0);

    /**
     * Debug log representation. Used by toString(const T*).
     */
    virtual QString idForToStringFromPtr() const;

    /** Limit chunks loading to only main or backup storage - or allow both (default). */
    virtual void setStorageLocation(nx::vms::api::StorageLocation /*value*/) {}

signals:
    /**
     * Emitted whenever motion periods were successfully loaded.
     * @param startTimeMs Start of the time period for the updated piece of data.
     */
    void ready(qint64 startTimeMs);

    /**
     * Emitted whenever the reader was unable to load motion periods.
     * @param handle Request handle.
     */
    void failed(int handle);

protected:
    /** Resource that this loader gets chunks for. */
    const QnResourcePtr m_resource;

    const Qn::TimePeriodContent m_dataType;

    /** Loaded data. */
    QnTimePeriodList m_loadedData;
};

} // namespace nx::vms::client::core
