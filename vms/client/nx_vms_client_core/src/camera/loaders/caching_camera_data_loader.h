// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_CACHING_CAMERA_DATA_LOADER_H
#define QN_CACHING_CAMERA_DATA_LOADER_H

#include <array>
#include <set>

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>

#include <analytics/db/analytics_db_types.h>
#include <camera/data/time_period_camera_data.h>
#include <camera/loaders/camera_data_loader_fwd.h>
#include <common/common_globals.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/common/data/motion_selection.h>
#include <recording/time_period.h>
#include <recording/time_period_list.h>
#include <utils/common/connective.h>


class QnCachingCameraDataLoader: public Connective<QObject> {
    Q_OBJECT;
    typedef Connective<QObject> base_type;

    using MotionSelection = nx::vms::client::core::MotionSelection;

public:
    QnCachingCameraDataLoader(const QnMediaResourcePtr& resource, QObject* parent = nullptr);
    virtual ~QnCachingCameraDataLoader() override;

    QnMediaResourcePtr resource() const;

    static bool supportedResource(const QnMediaResourcePtr &resource);

    void setMotionSelection(const MotionSelection& motionRegions);

    using AnalyticsFilter = nx::analytics::db::Filter;
    const AnalyticsFilter& analyticsFilter() const;
    void setAnalyticsFilter(const AnalyticsFilter& value);

    QnTimePeriodList periods(Qn::TimePeriodContent periodType) const;

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

private slots:
    void at_loader_ready(
        const QnAbstractCameraDataPtr& timePeriods,
        qint64 startTimeMs,
        Qn::TimePeriodContent dataType);

protected:
    bool loadInternal(Qn::TimePeriodContent periodType);

private:
    void init();
    void initLoaders();
    void updateTimePeriods(Qn::TimePeriodContent dataType, bool forced = false);

private:
    AllowedContent m_allowedContent;

    const QnMediaResourcePtr m_resource;

    QElapsedTimer m_previousRequestTime[Qn::TimePeriodContentCount];

    QnTimePeriodList m_cameraChunks[Qn::TimePeriodContentCount];

    std::array<QnAbstractCameraDataLoaderPtr, Qn::TimePeriodContentCount> m_loaders;

    MotionSelection m_motionSelection;
    AnalyticsFilter m_analyticsFilter;
};

typedef QSharedPointer<QnCachingCameraDataLoader> QnCachingCameraDataLoaderPtr;

#endif // QN_CACHING_CAMERA_DATA_LOADER_H
