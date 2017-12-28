#pragma once

#if defined(ENABLE_DESKTOP_CAMERA)

#include <network/tcp_connection_processor.h>

class QnDesktopCameraRegistratorPrivate;

class QnDesktopCameraRegistrator: public QnTCPConnectionProcessor
{
public:
    QnDesktopCameraRegistrator(QSharedPointer<nx::network::AbstractStreamSocket> socket, QnTcpListener* owner);

protected:
    virtual void run() override;
};

#endif // defined(ENABLE_DESKTOP_CAMERA)
