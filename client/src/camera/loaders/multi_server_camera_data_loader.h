#ifndef QN_MULTI_SERVER_CAMERA_DATA_LOADER_H
#define QN_MULTI_SERVER_CAMERA_DATA_LOADER_H

#include <QtCore/QMap>
#include <utils/common/mutex.h>

#include <common/common_globals.h>

#include <camera/data/camera_data_fwd.h>
#include <camera/loaders/abstract_camera_data_loader.h>

/**
 * Data loader that can be used to load data for a single camera from several mediaservers at once.
 */
class QnMultiServerCameraDataLoader: public QnAbstractCameraDataLoader
{
    Q_OBJECT
public:
    QnMultiServerCameraDataLoader(const QnResourcePtr &resource, Qn::CameraDataType dataType, QObject *parent = 0);
    static QnMultiServerCameraDataLoader *newInstance(const QnResourcePtr &resource, Qn::CameraDataType dataType, QObject *parent = 0);

    virtual int load(const QnTimePeriod &period, const QString &filter, const qint64 resolutionMs) override;

    virtual void discardCachedData(const qint64 resolutionMs = 0) override;

private slots:
    void onDataLoaded(const QnAbstractCameraDataPtr &data, int handle);
    void onLoadingFailed(int code, int handle);

private:
    int loadInternal(const QnMediaServerResourcePtr &server, const QnNetworkResourcePtr &camera, const QnTimePeriod &period, const QString &filter, const qint64 resolutionMs);

private:
    QMap<QString, QnAbstractCameraDataLoader *> m_cache;

    QMap<int, QList<int> > m_multiLoadProgress;
    QMap<int, QList<QnAbstractCameraDataPtr> > m_multiLoadData;
};

#endif // QN_MULTI_SERVER_CAMERA_DATA_LOADER_H
