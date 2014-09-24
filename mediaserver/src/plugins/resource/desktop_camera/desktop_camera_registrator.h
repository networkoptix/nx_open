#ifndef __DESKTOP_CAMERA_REGISTRATOR_H_
#define __DESKTOP_CAMERA_REGISTRATOR_H_

#ifdef ENABLE_DESKTOP_CAMERA

#include "utils/network/tcp_connection_processor.h"

class QnDesktopCameraRegistratorPrivate;

class QnDesktopCameraRegistrator: public QnTCPConnectionProcessor
{
public:
    QnDesktopCameraRegistrator(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* _owner);
    
    virtual bool isTakeSockOwnership() const override { return true; }
protected:
    virtual void run();
private:
    Q_DECLARE_PRIVATE(QnDesktopCameraRegistrator);
};

#endif //ENABLE_DESKTOP_CAMERA

#endif // __DESKTOP_CAMERA_REGISTRATOR_H_
