#ifndef __CAMERA_USER_DATA_H_
#define __CAMERA_USER_DATA_H_

#include "distributed_mutex.h"

namespace ec2
{

class QnMutexCameraDataHandler: public QnMutexUserDataHandler
{
    virtual QByteArray getUserData(const QString& name) override;
    virtual bool checkUserData(const QString& name, const QByteArray& data) override;
};

}

#endif // __CAMERA_USER_DATA_H_
