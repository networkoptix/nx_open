#ifndef __DESKTOP_CAMERA_REGISTRATOR_H_
#define __DESKTOP_CAMERA_REGISTRATOR_H_

#ifdef ENABLE_DESKTOP_CAMERA

#include "network/tcp_connection_processor.h"

class QnDesktopCameraRegistratorPrivate;

class QnDesktopCameraRegistrator: public QnTCPConnectionProcessor
{
public:
    QnDesktopCameraRegistrator(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner);

protected:
    virtual void run();
};

#endif //ENABLE_DESKTOP_CAMERA

#endif // __DESKTOP_CAMERA_REGISTRATOR_H_
