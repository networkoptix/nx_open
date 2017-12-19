#ifndef __TEST_CAMERA_CONNECTION_PROCESSOR_H__
#define __TEST_CAMERA_CONNECTION_PROCESSOR_H__

#include <network/tcp_connection_processor.h>

class QnTestCameraProcessorPrivate;

class QnTestCameraProcessor: public QnTCPConnectionProcessor
{
public:
    QnTestCameraProcessor(
        const QSharedPointer<AbstractStreamSocket>& socket,
        QnTcpListener* owner,
        bool noSecondaryStream);

    virtual ~QnTestCameraProcessor();
    virtual void run() override;

private:
    Q_DECLARE_PRIVATE(QnTestCameraProcessor);
    bool m_noSecondaryStream;
};

#endif // __TEST_CAMERA_CONNECTION_PROCESSOR_H__
