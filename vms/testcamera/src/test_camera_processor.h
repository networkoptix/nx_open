#ifndef __TEST_CAMERA_CONNECTION_PROCESSOR_H__
#define __TEST_CAMERA_CONNECTION_PROCESSOR_H__

#include <network/tcp_connection_processor.h>

class QnTestCameraProcessorPrivate;

class QnTestCameraProcessor: public QnTCPConnectionProcessor
{
public:
    /**
     * @param fpsPrimary If not -1, override the default value or the value received from Client.
     * @param fpsSecondary If not -1, override the default value or the value received from Client.
     */
    QnTestCameraProcessor(
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        QnTcpListener* owner,
        bool noSecondaryStream,
        int fpsPrimary,
        int fpsSecondary);

    virtual ~QnTestCameraProcessor();
    virtual void run() override;

private:
    Q_DECLARE_PRIVATE(QnTestCameraProcessor);
    bool m_noSecondaryStream;
    int m_fpsPrimary;
    int m_fpsSecondary;
};

#endif // __TEST_CAMERA_CONNECTION_PROCESSOR_H__
