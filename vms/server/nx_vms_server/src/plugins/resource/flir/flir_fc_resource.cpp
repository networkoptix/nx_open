#ifdef ENABLE_FLIR

#include "flir_fc_resource.h"
#include "flir_io_executor.h"

#include <nx/utils/log/log.h>
#include <utils/common/synctime.h>
#include <streaming/rtp_stream_reader.h>
#include <media_server/media_server_module.h>
#include <media_server/media_server_resource_searchers.h>
#include "flir_resource_searcher.h"

namespace {

const std::chrono::milliseconds kServerStatusRequestTimeout = std::chrono::seconds(40);
const std::chrono::milliseconds kServerStatusResponseTimeout = std::chrono::seconds(40);

} // namespace

namespace nx {
namespace plugins {
namespace flir {

const QString kDriverName = lit("FlirFC");

FcResource::FcResource(QnMediaServerModule* serverModule)
    :
    nx::vms::server::resource::Camera(serverModule),
    m_ioManager(nullptr),
    m_callbackIsInProgress(false)
{
}

FcResource::~FcResource()
{
    stopInputPortStatesMonitoring();

    if (m_ioManager)
    {
        QMetaObject::invokeMethod(
            m_ioManager,
            "deleteLater",
            Qt::QueuedConnection);
    }

    QnMutexLocker lock(&m_ioMutex);
    while (m_callbackIsInProgress)
        m_ioWaitCondition.wait(&m_ioMutex);
}

nx::vms::server::resource::StreamCapabilityMap FcResource::getStreamCapabilityMapFromDrives(
    Qn::StreamIndex /*streamIndex*/)
{
    // TODO: implement me
    return nx::vms::server::resource::StreamCapabilityMap();
}

CameraDiagnostics::Result FcResource::initializeCameraDriver()
{
    setCameraCapability(Qn::customMediaPortCapability, true);
    quint16 port = nexus::kDefaultNexusPort;
    nx::network::http::HttpClient httpClient;
    auto auth = getAuth();

    httpClient.setSendTimeout(kServerStatusRequestTimeout);
    httpClient.setMessageBodyReadTimeout(kServerStatusResponseTimeout);
    httpClient.setResponseReadTimeout(kServerStatusResponseTimeout);
    httpClient.setUserName(auth.user());
    httpClient.setUserPassword(auth.password());

    auto serverStatus = getNexusServerStatus(httpClient);

    // It's ok if this request fails. Everything can happen - it's Flir camera.
    // We just hope that everything is fine and use default settings.
    if (serverStatus)
    {
        if (!serverStatus->isNexusServerEnabled)
        {
            bool serverEnabled = tryToEnableNexusServer(httpClient);
            if (!serverEnabled)
            {
                return CameraDiagnostics::RequestFailedResult(
                    lit("Enable Nexus server"),
                    lit("Failed to enable Nexus server"));
            }
        }

        const auto& settings = serverStatus->settings;
        const auto& group = settings.find(fc_private::kNexusInterfaceGroupName);
        if (group != settings.cend())
        {
            const auto& nexusPortParameter = group->second.find(fc_private::kNexusPortParamName);
            if (nexusPortParameter != group->second.cend())
            {
                bool status = false;
                const auto nexusPort = nexusPortParameter->second
                    .toInt(&status);

                if (status)
                    port = nexusPort;
            }
        }
    }

    if (!m_ioManager)
    {
        m_ioManager = new nexus::WebSocketIoManager(this, port);
        auto flirSearcher = serverModule()->resourceSearchers()->searcher<QnFlirResourceSearcher>();
        m_ioManager->moveToThread(flirSearcher->ioExecutor()->getThread());
    }

    QnIOPortDataList allPorts = m_ioManager->getInputPortList();
    QnIOPortDataList outputPorts = m_ioManager->getOutputPortList();
    allPorts.insert(allPorts.begin(), outputPorts.begin(), outputPorts.end());

    setIoPortDescriptions(std::move(allPorts), /*needMerge*/ true);
    saveProperties();
    return CameraDiagnostics::NoErrorResult();
}

void FcResource::startInputPortStatesMonitoring()
{
    if (!m_ioManager)
        return;

    if (m_ioManager->isMonitoringInProgress())
        return;

    m_ioManager->setInputPortStateChangeCallback(
        [this](QString portId, nx_io_managment::IOPortState portState)
        {
            QnMutexLocker lock(&m_ioMutex);
            m_callbackIsInProgress = true;

            lock.unlock();
            emit inputPortStateChanged(
                toSharedPointer(this),
                portId,
                nx_io_managment::isActiveIOPortState(portState),
                qnSyncTime->currentUSecsSinceEpoch());

            lock.relock();
            m_callbackIsInProgress = false;
            m_ioWaitCondition.wakeAll();
        });

    m_ioManager->setNetworkIssueCallback(
        [this](QString reason, bool isFatal)
        {
            QnMutexLocker lock(&m_ioMutex);
            m_callbackIsInProgress = true;

            lock.unlock();
            const auto logLevel = isFatal ? utils::log::Level::warning : utils::log::Level::error;
            NX_UTILS_LOG(logLevel, this,
                lm("Flir Onvif resource, %1 (%2), netowk issue detected. Reason: %3")
                    .arg(getModel())
                    .arg(getUrl())
                    .arg(reason));

            lock.relock();
            m_callbackIsInProgress = false;
            m_ioWaitCondition.wakeAll();
        });

    m_ioManager->startIOMonitoring();
}

void FcResource::stopInputPortStatesMonitoring()
{
    if (!m_ioManager)
        return;

    if (!m_ioManager->isMonitoringInProgress())
        return;

    m_ioManager->stopIOMonitoring();
}

QnAbstractStreamDataProvider* FcResource::createLiveDataProvider()
{
    auto reader = new QnRtpStreamReader(toSharedPointer(this), "ch0");
    reader->setRtpTransport(RtspTransport::udp);

    return reader;
}

QString FcResource::getDriverName() const
{
    return kDriverName;
}

void FcResource::setIframeDistance(int, int)
{
    // Do nothing.
}

bool FcResource::hasDualStreamingInternal() const
{
    return false;
}

bool FcResource::doGetRequestAndCheckResponse(nx::network::http::HttpClient& httpClient, const nx::utils::Url& url)
{
    auto success = httpClient.doGet(url);
    if (!success)
        return false;

    auto response = httpClient.response();

    if (response->statusLine.statusCode != nx::network::http::StatusCode::ok)
        return false;

    return true;
}

boost::optional<fc_private::ServerStatus> FcResource::getNexusServerStatus(nx::network::http::HttpClient& httpClient)
{
    nx::utils::Url url = getUrl();
    url.setPath(fc_private::kConfigurationFile);

    if (!doGetRequestAndCheckResponse(httpClient, url))
        return boost::none;

    nx::network::http::BufferType messageBody;
    while (!httpClient.eof())
        messageBody.append(httpClient.fetchMessageBodyBuffer());

    return parseNexusServerStatusResponse(QString::fromUtf8(messageBody));
}

bool FcResource::tryToEnableNexusServer(nx::network::http::HttpClient& httpClient)
{
    nx::utils::Url url = getUrl();
    url.setPath(fc_private::kStartNexusServerCommand);

    if (!doGetRequestAndCheckResponse(httpClient, url))
        return false;

    nx::network::http::BufferType messageBody;
    while (!httpClient.eof())
        messageBody.append(httpClient.fetchMessageBodyBuffer());

    bool status = false;
    auto isEnabled = messageBody.trimmed().toInt(&status);

    if (!status || isEnabled != 1)
        return false;

    return true;
}

bool FcResource::setOutputPortState(
    const QString& outputId,
    bool isActive,
    unsigned int autoResetTimeoutMs)
{
    QnMutexLocker lock(&m_mutex);

    if (!m_ioManager)
        return false;

    if (!isActive)
    {
        for (auto it = m_autoResetTimers.begin(); it != m_autoResetTimers.end(); ++it)
        {
            auto timerId = it->first;
            auto portTimerEntry = it->second;
            if (it->second.portId == outputId)
            {
                nx::utils::TimerManager::instance()->deleteTimer(timerId);
                it = m_autoResetTimers.erase(it);
                break;
            }
        }
    }

    if (isActive && autoResetTimeoutMs)
    {
        auto autoResetTimer = nx::utils::TimerManager::instance()->addTimer(
            [this](quint64  timerId)
        {
            QnMutexLocker lock(&m_mutex);
            if (m_autoResetTimers.count(timerId))
            {
                auto timerEntry = m_autoResetTimers[timerId];
                m_ioManager->setOutputPortState(
                    timerEntry.portId,
                    timerEntry.state);
            }
            m_autoResetTimers.erase(timerId);
        },
            std::chrono::milliseconds(autoResetTimeoutMs));

        PortTimerEntry portTimerEntry;
        portTimerEntry.portId = outputId;
        portTimerEntry.state = !isActive;
        m_autoResetTimers[autoResetTimer] = portTimerEntry;
    }

    return m_ioManager->setOutputPortState(outputId, isActive);
}

} // namespace flir
} // namespace plugins
} // namespace nx

#endif // ENABLE_FLIR
