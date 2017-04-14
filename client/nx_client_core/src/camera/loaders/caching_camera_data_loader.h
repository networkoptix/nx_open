#ifndef QN_CACHING_CAMERA_DATA_LOADER_H
#define QN_CACHING_CAMERA_DATA_LOADER_H

#include <array>

#include <QtCore/QObject>
#include <QtCore/QElapsedTimer>

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

    QnTimePeriodList periods(Qn::TimePeriodContent periodType) const;

    void load(bool forced = false);

    void setEnabled(bool value);
    bool enabled() const;

    void updateServer(const QnMediaServerResourcePtr& server);

signals:
    void periodsChanged(Qn::TimePeriodContent type, qint64 startTimeMs = 0);
    void loadingFailed();
public slots:
    void discardCachedData();
private slots:
    void at_loader_ready(const QnAbstractCameraDataPtr &timePeriods, qint64 startTimeMs, Qn::TimePeriodContent dataType);

protected:
    bool loadInternal(Qn::TimePeriodContent periodType);

private:
    void init();
    void initLoaders();
    void updateTimePeriods(Qn::TimePeriodContent dataType, bool forced = false);
    void trace(const QString& message, Qn::TimePeriodContent periodType = Qn::RecordingContent);

private:
    bool m_enabled = false;

    const QnMediaResourcePtr m_resource;
    QnMediaServerResourcePtr m_server;

    QElapsedTimer m_previousRequestTime[Qn::TimePeriodContentCount];

    QnTimePeriodList m_cameraChunks[Qn::TimePeriodContentCount];

    std::array<QnAbstractCameraDataLoaderPtr, Qn::TimePeriodContentCount> m_loaders;

    QList<QRegion> m_motionRegions;
};

typedef QSharedPointer<QnCachingCameraDataLoader> QnCachingCameraDataLoaderPtr;

#endif // QN_CACHING_CAMERA_DATA_LOADER_H
