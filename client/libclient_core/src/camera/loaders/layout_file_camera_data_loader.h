#ifndef QN_LAYOUT_FILE_CAMERA_DATA_LOADER_H
#define QN_LAYOUT_FILE_CAMERA_DATA_LOADER_H

#include <QtCore/QMap>
#include <nx/utils/thread/mutex.h>

#include <camera/data/camera_data_fwd.h>
#include <common/common_globals.h>
#include <camera/loaders/abstract_camera_data_loader.h>


/**
 * Time period loader that can be used to load recorded time periods from exported layout
 * cameras.
 */

class QnLayoutFileCameraDataLoader: public QnAbstractCameraDataLoader
{
public:
    QnLayoutFileCameraDataLoader(const QnAviResourcePtr &resource, Qn::TimePeriodContent dataType, QObject *parent = NULL);
    virtual ~QnLayoutFileCameraDataLoader();
    virtual int load(const QString &filter, const qint64 resolutionMs) override;
private:
    int sendDataDelayed(const QnAbstractCameraDataPtr& data);


    int loadChunks();
    int loadMotion(const QList<QRegion> &motionRegions);
private:
    const QnAviResourcePtr m_aviResource;
    QnAbstractCameraDataPtr m_data;
};

#endif
