#ifndef QN_CACHING_CAMERA_DATA_LOADER_H
#define QN_CACHING_CAMERA_DATA_LOADER_H

#include <array>
#include <set>

#include <QtCore/QObject>
#include <QtCore/QElapsedTimer>

#include <analytics/db/analytics_db_types.h>
#include <camera/data/time_period_camera_data.h>
#include <camera/loaders/camera_data_loader_fwd.h>
#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <recording/time_period.h>
#include <recording/time_period_list.h>
#include <utils/common/connective.h>


class QnCachingCameraDataLoader: public Connective<QObject> {
    Q_OBJECT;

    typedef Connective<QObject> base_type;
public:
    virtual ~QnCachingCameraDataLoader();

    QnCachingCameraDataLoader(const QnMediaResourcePtr &resource,
        const QnMediaServerResourcePtr& server,
        QObject *parent = NULL);

    QnMediaResourcePtr resource() const;

    static bool supportedResource(const QnMediaResourcePtr &resource);

    const QList<QRegion> &motionRegions() const;
    void setMotionRegions(const QList<QRegion> &motionRegions);
    bool isMotionRegionsEmpty() const;

    using AnalyticsFilter = nx::analytics::db::Filter;
    const AnalyticsFilter& analyticsFilter() const;
    void setAnalyticsFilter(const AnalyticsFilter& value);

    QnTimePeriodList periods(Qn::TimePeriodContent periodType) const;

    void load(bool forced = false);

    using AllowedContent = std::set<Qn::TimePeriodContent>;
    AllowedContent allowedContent() const;
    void setAllowedContent(AllowedContent value);
    bool isContentAllowed(Qn::TimePeriodContent content) const;

    void updateServer(const QnMediaServerResourcePtr& server);

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
    void at_loader_ready(const QnAbstractCameraDataPtr &timePeriods, qint64 startTimeMs, Qn::TimePeriodContent dataType);

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

    QnTimePeriodList m_cameraChunks[Qn::TimePeriodContentCount];

    std::array<QnAbstractCameraDataLoaderPtr, Qn::TimePeriodContentCount> m_loaders;

    QList<QRegion> m_motionRegions;
    AnalyticsFilter m_analyticsFilter;
};

typedef QSharedPointer<QnCachingCameraDataLoader> QnCachingCameraDataLoaderPtr;

#endif // QN_CACHING_CAMERA_DATA_LOADER_H
