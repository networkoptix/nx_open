#ifndef QN_LAYOUT_FILE_CAMERA_DATA_LOADER_H
#define QN_LAYOUT_FILE_CAMERA_DATA_LOADER_H

#include <QtCore/QMap>
#include <utils/thread/mutex.h>

#include <camera/data/camera_data_fwd.h>
#include <camera/loaders/abstract_camera_data_loader.h>

#include <common/common_globals.h>

/**
 * Time period loader that can be used to load recorded time periods from exported layout
 * cameras.
 */

class QnLayoutFileCameraDataLoader: public QnAbstractCameraDataLoader
{
public:
    QnLayoutFileCameraDataLoader(const QnAviResourcePtr &resource, Qn::CameraDataType dataType, QObject *parent);
    QnLayoutFileCameraDataLoader(const QnAviResourcePtr &resource, Qn::CameraDataType dataType, const QnAbstractCameraDataPtr& data, QObject *parent);
    virtual ~QnLayoutFileCameraDataLoader();
    static QnLayoutFileCameraDataLoader* newInstance(const QnAviResourcePtr &resource, Qn::CameraDataType dataType, QObject *parent = 0);
    virtual int load(const QString &filter, const qint64 resolutionMs) override;
private:
    int loadChunks();
    int loadMotion(const QList<QRegion> &motionRegions);
private:
    QnAbstractCameraDataPtr m_data;
};

#endif // QN_LAYOUT_FILE_CAMERA_DATA_LOADER_H
