#ifndef __TEST_CAMERA_CONNECTION_PROCESSOR_H__
#define __TEST_CAMERA_CONNECTION_PROCESSOR_H__

#include "utils/network/tcp_connection_processor.h"

class QnTestCameraProcessor: public QnTCPConnectionProcessor
{
public:
    QnTestCameraProcessor(TCPSocket* socket, QnTcpListener* owner);
    virtual void run() override;
private:
    QN_DECLARE_PRIVATE_DERIVED(QnTestCameraProcessor);
};

#endif // __TEST_CAMERA_CONNECTION_PROCESSOR_H__
