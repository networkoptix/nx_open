#ifdef ENABLE_FLIR

#include "flir_fc_resource.h"
#include "flir_io_executor.h"

#include <nx/utils/log/log.h>
#include <utils/common/synctime.h>
#include <plugins/resource/onvif/dataprovider/rtp_stream_provider.h>

using namespace nx::plugins::flir;

namespace {

const QString kFlirFcDriverName = lit("FlirFC");
const std::chrono::milliseconds kServerStatusRequestTimeout = std::chrono::seconds(40);
const std::chrono::milliseconds kServerStatusResponseTimeout = std::chrono::seconds(40);

} // namespace

FcResource::FcResource():
    m_ioManager(nullptr)
{
}

FcResource::~FcResource()
{
    stopInputPortMonitoringAsync();

    if (m_ioManager)
    {
        QMetaObject::invokeMethod(
            m_ioManager,
            "deleteLater",
            Qt::QueuedConnection);
    }
}

CameraDiagnostics::Result FcResource::initInternal()
{
    quint16 port = nexus::kDefaultNexusPort;
    nx_http::HttpClient httpClient;
    auto auth = getAuth();

    httpClient.setSendTimeoutMs(kServerStatusRequestTimeout.count());
    httpClient.setMessageBodyReadTimeoutMs(kServerStatusResponseTimeout.count());
    httpClient.setResponseReadTimeoutMs(kServerStatusResponseTimeout.count());
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

        const auto& kSettings = serverStatus->settings;
        if (kSettings.find(fc_private::kNexusInterfaceGroupName) != kSettings.cend())
        {
            const auto& group = kSettings.at(fc_private::kNexusInterfaceGroupName);
            if (group.find(fc_private::kNexusPortParamName) != group.cend())
            {
                bool status = false;
                const auto kNexusPort = group
                    .at(fc_private::kNexusPortParamName)
                    .toInt(&status);

                if (status)
                    port = kNexusPort;
            }
        }
    }

    if (!m_ioManager)
    {
        m_ioManager = new nexus::WebSocketIoManager(this, port);
        m_ioManager->moveToThread(IoExecutor::instance()->getThread());
    }

    Qn::CameraCapabilities caps = Qn::NoCapabilities;
    QnIOPortDataList allPorts = getInputPortList();
    QnIOPortDataList outputPorts = getRelayOutputList();

    if (!allPorts.empty())
        caps |= Qn::RelayInputCapability;

    if (!outputPorts.empty())
        caps |= Qn::RelayOutputCapability;

    allPorts.insert(allPorts.begin(), outputPorts.begin(), outputPorts.end());

    setIOPorts(allPorts);
    setCameraCapabilities(caps);

    saveParams();

    return CameraDiagnostics::NoErrorResult();
}

bool FcResource::startInputPortMonitoringAsync(std::function<void(bool)>&& completionHandler)
{
    if (!m_ioManager)
        return false;

    if (m_ioManager->isMonitoringInProgress())
        return false;

    m_ioManager->setInputPortStateChangeCallback(
        [this](QString portId, nx_io_managment::IOPortState portState)
        {
            emit cameraInput(
                toSharedPointer(this),
                portId,
                nx_io_managment::isActiveIOPortState(portState),
                qnSyncTime->currentUSecsSinceEpoch());
        });

    m_ioManager->setNetworkIssueCallback(
        [this](QString reason, bool /*isFatal*/)
        {
            NX_LOG(
                lm("Flir Onvif resource, %1 (%2), netowk issue detected. Reason: %3")
                .arg(getModel())
                .arg(getUrl())
                .arg(reason),
                cl_logWARNING);
        });

    m_ioManager->startIOMonitoring();
    return true;
}

void FcResource::stopInputPortMonitoringAsync()
{
    if (!m_ioManager)
        return;

    if (!m_ioManager->isMonitoringInProgress())
        return;

    m_ioManager->stopIOMonitoring();
}

QnIOPortDataList FcResource::getRelayOutputList() const
{
    if (m_ioManager)
        return m_ioManager->getOutputPortList();    

    return QnIOPortDataList();
}

QnIOPortDataList FcResource::getInputPortList() const
{
    if (m_ioManager)
        return m_ioManager->getInputPortList();

    return QnIOPortDataList();
}

QnAbstractStreamDataProvider* FcResource::createLiveDataProvider()
{
    auto reader = new QnRtpStreamReader(toSharedPointer(this), "ch0");
    reader->setRtpTransport(RtpTransport::udp);

    return reader;
}

QString FcResource::getDriverName() const
{
    return kFlirFcDriverName;
}

void FcResource::setIframeDistance(int, int)
{
    // Do nothing.
}

bool FcResource::hasDualStreaming() const
{
    return false;
}

bool FcResource::doGetRequestAndCheckResponse(nx_http::HttpClient& httpClient, const QUrl& url)
{
    auto success = httpClient.doGet(url);
    if (!success)
        return false;

    auto response = httpClient.response();

    if (response->statusLine.statusCode != nx_http::StatusCode::ok)
        return false;

    return true;
}

boost::optional<fc_private::ServerStatus> FcResource::getNexusServerStatus(nx_http::HttpClient& httpClient)
{
    QUrl url = getUrl();
    url.setPath(fc_private::kConfigurationFile);

    if (!doGetRequestAndCheckResponse(httpClient, url))
        return boost::none;

    nx_http::BufferType messageBody;
    while (!httpClient.eof())
        messageBody.append(httpClient.fetchMessageBodyBuffer());

    return parseNexusServerStatusResponse(QString::fromUtf8(messageBody));
}

bool FcResource::tryToEnableNexusServer(nx_http::HttpClient& httpClient)
{
    QUrl url = getUrl();
    url.setPath(fc_private::kStartNexusServerCommand);

    if (!doGetRequestAndCheckResponse(httpClient, url))
        return false;

    nx_http::BufferType messageBody;
    while (!httpClient.eof())
        messageBody.append(httpClient.fetchMessageBodyBuffer());
    
    bool status = false;
    auto isEnabled = messageBody.trimmed().toInt(&status);

    if (!status || !isEnabled == 1)
        return false;

    return true;
}

bool nx::plugins::flir::FcResource::setRelayOutputState(
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

#endif // ENABLE_FLIR
