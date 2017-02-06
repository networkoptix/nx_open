#ifndef __CAMERA_USER_DATA_H_
#define __CAMERA_USER_DATA_H_

#include "distributed_mutex.h"

namespace ec2
{

class QnMutexCameraDataHandler: public QnMutexUserDataHandler
{
public:
    const static QString CAM_INS_PREFIX;
    const static QString CAM_UPD_PREFIX;
    const static QString CAM_HISTORY_PREFIX;

    virtual QByteArray getUserData(const QString& name) override;
    virtual bool checkUserData(const QString& name, const QByteArray& data) override;
};

}

#endif // __CAMERA_USER_DATA_H_
