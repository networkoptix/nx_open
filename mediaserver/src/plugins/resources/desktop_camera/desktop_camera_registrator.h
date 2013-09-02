#ifndef __DESKTOP_CAMERA_REGISTRATOR_H_
#define __DESKTOP_CAMERA_REGISTRATOR_H_

#include "utils/network/tcp_connection_processor.h"

class QnDesktopCameraRegistratorPrivate;

class QnDesktopCameraRegistrator: public QnTCPConnectionProcessor
{
public:
    QnDesktopCameraRegistrator(TCPSocket* socket, QnTcpListener* _owner);
protected:
    virtual void run();
private:
    Q_DECLARE_PRIVATE(QnDesktopCameraRegistrator);
};

#endif // __DESKTOP_CAMERA_REGISTRATOR_H_
