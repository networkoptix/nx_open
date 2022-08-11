// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>
#include <set>

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>

#include <analytics/db/analytics_db_types.h>
#include <common/common_globals.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/api/types/storage_location.h>
#include <nx/vms/client/core/common/data/motion_selection.h>
#include <recording/time_period.h>
#include <recording/time_period_list.h>

#include "camera_data_loader_fwd.h"

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API CachingCameraDataLoader: public QObject {
    Q_OBJECT;
    typedef QObject base_type;

    using MotionSelection = nx::vms::client::core::MotionSelection;

public:
    CachingCameraDataLoader(const QnMediaResourcePtr& resource, QObject* parent = nullptr);
    virtual ~CachingCameraDataLoader() override;

    QnMediaResourcePtr resource() const;

    static bool supportedResource(const QnMediaResourcePtr &resource);

    void setMotionSelection(const MotionSelection& motionRegions);

    using AnalyticsFilter = nx::analytics::db::Filter;
    const AnalyticsFilter& analyticsFilter() const;
    void setAnalyticsFilter(const AnalyticsFilter& value);

    /** Limit chunks loading to only main or backup storage - or allow both (default). */
    void setStorageLocation(nx::vms::api::StorageLocation value);

    const QnTimePeriodList& periods(Qn::TimePeriodContent periodType) const;

    void load(bool forced = false);

    using AllowedContent = std::set<Qn::TimePeriodContent>;
    AllowedContent allowedContent() const;
    void setAllowedContent(AllowedContent value);
    bool isContentAllowed(Qn::TimePeriodContent content) const;

    /**
     * Debug log representation. Used by toString(const T*).
     */
    QString idForToStringFromPtr() const;

signals:
    void periodsChanged(Qn::TimePeriodContent type, qint64 startTimeMs = 0);
    void loadingFailed();

public slots:
    void discardCachedData();
    void discardCachedDataType(Qn::TimePeriodContent type);
    void invalidateCachedData();

protected:
    bool loadInternal(Qn::TimePeriodContent periodType);

private:
    void init();
    void initLoaders();
    void updateTimePeriods(Qn::TimePeriodContent dataType, bool forced = false);

private:
    AllowedContent m_allowedContent;

    const QnMediaResourcePtr m_resource;
    QnMediaServerResourcePtr m_server;

    QElapsedTimer m_previousRequestTime[Qn::TimePeriodContentCount];

    std::array<AbstractCameraDataLoaderPtr, Qn::TimePeriodContentCount> m_loaders;

    MotionSelection m_motionSelection;
    AnalyticsFilter m_analyticsFilter;
    nx::vms::api::StorageLocation m_storageLocation = nx::vms::api::StorageLocation::both;
};

using CachingCameraDataLoaderPtr = QSharedPointer<CachingCameraDataLoader>;

} // namespace nx::vms::client::core
