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
    
    typedef Connective<QObject> base_type;
public: 
    virtual ~QnCachingCameraDataLoader();

    static QnCachingCameraDataLoader *newInstance(const QnResourcePtr &resource, QObject *parent = NULL);

    QnResourcePtr resource() const;
        
    const QList<QRegion> &motionRegions() const;
    void setMotionRegions(const QList<QRegion> &motionRegions);
    bool isMotionRegionsEmpty() const;

    QnTimePeriodList periods(Qn::TimePeriodContent periodType) const;
    QnCameraBookmarkList bookmarks() const;

    QString bookmarksTextFilter() const;
    void setBookmarksTextFilter(const QString &filter);

    void addBookmark(const QnCameraBookmark &bookmark);
    void updateBookmark(const QnCameraBookmark &bookmark);
    void removeBookmark(const QnCameraBookmark & bookmark);
    QnCameraBookmark bookmarkByTime(qint64 position) const;
    QnCameraBookmarkList allBookmarksByTime(qint64 position) const;


    void load(bool forced = false);

    void setEnabled(bool value);
    bool enabled() const;
signals:
    void periodsChanged(Qn::TimePeriodContent type, qint64 startTimeMs = 0);
    void bookmarksChanged();
    void loadingFailed();
public slots:
    void discardCachedData();
private slots:
    void at_loader_ready(const QnAbstractCameraDataPtr &timePeriods, qint64 startTimeMs, Qn::CameraDataType dataType);

protected:
    void loadInternal(Qn::TimePeriodContent periodType);
    
private:
    QnCachingCameraDataLoader(const QnResourcePtr &resource, QnAbstractCameraDataLoader **loaders, QObject *parent);

    void init();
    void initLoaders(QnAbstractCameraDataLoader **loaders);
    static bool createLoaders(const QnResourcePtr &resource, QnAbstractCameraDataLoader **loaders);
    
    qint64 bookmarkResolution(qint64 periodDuration) const;
    void updateTimePeriods(Qn::TimePeriodContent dataType, bool forced = false);
    void updateBookmarks();
    
private:
    bool m_enabled;

    QnResourcePtr m_resource;
  
    QElapsedTimer m_previousRequestTime[Qn::TimePeriodContentCount];

    QnTimePeriodList m_cameraChunks[Qn::TimePeriodContentCount];

    QMap<qint64, QnTimePeriodList> m_requestedBookmarkPeriodsByResolution;  //TODO: #GDM #Bookmarks should we enumerate by resolution set index?
    QnBookmarkCameraData m_bookmarkCameraData;

    QnAbstractCameraDataLoader *m_loaders[Qn::CameraDataTypeCount];

    QList<QRegion> m_motionRegions;
    QString m_bookmarksTextFilter;
};


#endif // QN_CACHING_CAMERA_DATA_LOADER_H
