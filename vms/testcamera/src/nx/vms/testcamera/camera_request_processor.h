#pragma once

#include <optional>

#include <network/tcp_connection_processor.h>

namespace nx::vms::testcamera {

class Logger; //< private
class CameraPool;
class CameraRequestProcessorPrivate;

/**
 * Runs the thread which handles the streaming requests from Servers: receives a URL from a Server,
 * finds the respective Camera in the CameraPool, and asks it to perform streaming for the Server.
 */
class CameraRequestProcessor: public QnTCPConnectionProcessor
{
public:
    /**
     * @param fpsPrimary If set, overrides the default value or the value received from Client.
     * @param fpsSecondary If set, overrides the default value or the value received from Client.
     */
    CameraRequestProcessor(
        CameraPool* cameraPool,
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        bool noSecondaryStream,
        std::optional<int> fpsPrimary,
        std::optional<int> fpsSecondary);

    virtual ~CameraRequestProcessor();

    virtual void run() override;

private:
    Q_DECLARE_PRIVATE(CameraRequestProcessor);

    /** @return Empty string on error, having logged the error message. */
    QByteArray receiveCameraUrl();

private:
    const std::unique_ptr<Logger> m_logger;
    CameraPool* const m_cameraPool;
    bool m_noSecondaryStream;
    std::optional<int> m_fpsPrimary;
    std::optional<int> m_fpsSecondary;
};

} // namespace nx::vms::testcamera
