#ifndef QN_MULTI_SERVER_CAMERA_DATA_LOADER_H
#define QN_MULTI_SERVER_CAMERA_DATA_LOADER_H

#include <QtCore/QMap>
#include <QtCore/QMutex>

#include <common/common_globals.h>

#include <camera/data/camera_data_fwd.h>
#include <camera/loaders/abstract_camera_data_loader.h>

#include <recording/time_period.h>

/**
 * Data loader that can be used to load data for a single camera from several servers at once.
 */
class QnMultiServerCameraDataLoader: public QnAbstractCameraDataLoader
{
    Q_OBJECT
public:
    QnMultiServerCameraDataLoader(const QnResourcePtr &resource, Qn::CameraDataType dataType, QObject *parent = 0);
    static QnMultiServerCameraDataLoader *newInstance(const QnResourcePtr &resource, Qn::CameraDataType dataType, QObject *parent = 0);

    virtual int load(const QString &filter, const qint64 resolutionMs) override;

    virtual void discardCachedData(const qint64 resolutionMs = 0) override;

private slots:
    void onDataLoaded(const QnAbstractCameraDataPtr &data, const QnTimePeriod &updatedPeriod, int handle);
    void onLoadingFailed(int code, int handle);

private:
    int loadInternal(const QnMediaServerResourcePtr &server, const QnNetworkResourcePtr &camera, const QString &filter, const qint64 resolutionMs);

private:

    struct LoadingInfo {
        QList<QnAbstractCameraDataPtr> data;
        qint64 startTimeMs;

        LoadingInfo(): startTimeMs(0) {}
    };

    QMap<QString, QnAbstractCameraDataLoader *> m_cache;

    QMap<int, QList<int> > m_multiLoadProgress;
    QMap<int, LoadingInfo > m_multiLoadData;
};

#endif // QN_MULTI_SERVER_CAMERA_DATA_LOADER_H
