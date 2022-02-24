// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_LAYOUT_FILE_CAMERA_DATA_LOADER_H
#define QN_LAYOUT_FILE_CAMERA_DATA_LOADER_H

#include <QtCore/QMap>
#include <nx/utils/thread/mutex.h>

#include <camera/data/camera_data_fwd.h>
#include <common/common_globals.h>
#include <camera/loaders/abstract_camera_data_loader.h>

#include <nx/vms/client/core/common/data/motion_selection.h>

/**
 * Time period loader that can be used to load recorded time periods from exported layout
 * cameras.
 */
class QnLayoutFileCameraDataLoader: public QnAbstractCameraDataLoader
{
public:
    QnLayoutFileCameraDataLoader(const QnAviResourcePtr &resource, Qn::TimePeriodContent dataType, QObject *parent = nullptr);
    virtual ~QnLayoutFileCameraDataLoader();
    virtual int load(const QString &filter, const qint64 resolutionMs) override;

private:
    int sendDataDelayed(const QnAbstractCameraDataPtr& data);

    int loadChunks();
    int loadMotion(const nx::vms::client::core::MotionSelection& motionSelection);
private:
    const QnAviResourcePtr m_aviResource;
    QnAbstractCameraDataPtr m_data;
};

#endif
