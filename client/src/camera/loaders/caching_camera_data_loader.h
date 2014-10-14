#ifndef QN_CACHING_CAMERA_DATA_LOADER_H
#define QN_CACHING_CAMERA_DATA_LOADER_H

#include <QtCore/QObject>

#include <camera/data/bookmark_camera_data.h>
#include <camera/data/time_period_camera_data.h>

#include <common/common_globals.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>

#include <recording/time_period.h>
#include <recording/time_period_list.h>

#include <utils/common/connective.h>

class QnAbstractCameraDataLoader;

class QnCachingCameraDataLoader: public Connective<QObject> {
    Q_OBJECT;
    Q_PROPERTY(qreal loadingMargin READ loadingMargin WRITE setLoadingMargin);
    
    typedef Connective<QObject> base_type;
public:
    QnCachingCameraDataLoader(const QnResourcePtr &networkResource, QObject *parent = NULL);
    virtual ~QnCachingCameraDataLoader();

    static QnCachingCameraDataLoader *newInstance(const QnResourcePtr &resource, QObject *parent = NULL);

    QnResourcePtr resource() const;

    qreal loadingMargin() const;
    void setLoadingMargin(qreal loadingMargin);

    qint64 updateInterval() const;
    void setUpdateInterval(qint64 msecs);

    QnTimePeriod boundingPeriod() const;
    void setBoundingPeriod(const QnTimePeriod &boundingPeriod);

    QnTimePeriod targetPeriod(Qn::CameraDataType dataType) const;
    void setTargetPeriod(const QnTimePeriod &targetPeriod, Qn::CameraDataType dataType);
    
    const QList<QRegion> &motionRegions() const;
    void setMotionRegions(const QList<QRegion> &motionRegions);
    bool isMotionRegionsEmpty() const;

    QnTimePeriodList periods(Qn::TimePeriodContent type) const;
    QnCameraBookmarkList bookmarks() const;

    QString bookmarksTextFilter() const;
    void setBookmarksTextFilter(const QString &filter);

    void addBookmark(const QnCameraBookmark &bookmark);
    void updateBookmark(const QnCameraBookmark &bookmark);
    void removeBookmark(const QnCameraBookmark & bookmark);
    QnCameraBookmark bookmarkByTime(qint64 position) const;
signals:
    void periodsChanged(Qn::TimePeriodContent type);
    void bookmarksChanged();
    void loadingFailed();

private slots:
    void at_loader_ready(const QnAbstractCameraDataPtr &timePeriods, int handle);
    void at_loader_failed(int status, int handle);
    void discardCachedData();

protected:
    void load(Qn::CameraDataType type, const QnTimePeriod &targetPeriod, const qint64 resolutionMs = 1);
    
    QnTimePeriod addLoadingMargins(const QnTimePeriod &targetPeriod, const QnTimePeriod &boundingPeriod, const qint64 minMargin) const;

private:
    QnCachingCameraDataLoader(QnAbstractCameraDataLoader **loaders, QObject *parent);

    void init();
    void initLoaders(QnAbstractCameraDataLoader **loaders);
    static bool createLoaders(const QnResourcePtr &resource, QnAbstractCameraDataLoader **loaders);
    
    qint64 bookmarkResolution(qint64 periodDuration) const;
    void updateTimePeriods(Qn::CameraDataType dataType);
    void updateBookmarks();
    
private:
    QnResourcePtr m_resource;
    bool m_resourceIsLocal;

    qreal m_loadingMargin;
    qint64 m_updateInterval;

    QnTimePeriod m_targetPeriod[Qn::CameraDataTypeCount];
    QnTimePeriod m_boundingPeriod;

    QnTimePeriodList m_requestedTimePeriods[Qn::TimePeriodContentCount];
    QnTimePeriodCameraData m_timePeriodCameraData[Qn::TimePeriodContentCount];

    QMap<qint64, QnTimePeriodList> m_requestedBookmarkPeriodsByResolution;  //TODO: #GDM #Bookmarks should we enumerate by resolution set index?
    QnBookmarkCameraData m_bookmarkCameraData;

    QnAbstractCameraDataLoader *m_loaders[Qn::CameraDataTypeCount];
    int m_handles[Qn::CameraDataTypeCount];

    QList<QRegion> m_motionRegions;
    QString m_bookmarksTextFilter;
};


#endif // QN_CACHING_CAMERA_DATA_LOADER_H
