#include <chrono>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

#include "flir_ws_io_manager.h"
#include "flir_io_executor.h"
#include "flir_nexus_common.h"
#include "flir_nexus_parsing_utils.h"
#include "flir_nexus_string_builder.h"

#include <core/resource/camera_resource.h>
#include <common/common_module.h>
#include <core/resource_management/resource_data_pool.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/to_string.h>

namespace {
using ErrorSignalType = void(QWebSocket::*)(QAbstractSocket::SocketError);

//This group of settings is from a private API. We shouldn't rely on it.
const QString kConfigurationFile("/api/server/status/full");
const QString kStartNexusServerCommand("/api/server/start");
const QString kStopNexusServerCommand("/api/server/stop");
const QString kServerSuccessfullyStarted("1");
const QString kServerSuccessfullyStopped("0");
const QString kNumberOfInputsParamName("Number of IOs");
const QString kNexusInterfaceGroupName("INTERFACE Configuration - Device 0");
const QString kNexusPortParamName("Port");

const int kInvalidSessionId = -1;
const std::size_t kMaxThgAreasNumber = 4;
const std::size_t kMaxThgSpotsNumber = 8;
const std::size_t kMaxAlarmsNumber = 10;

const quint16 kDefaultNexusPort = 8080;

// Timeouts is so big because Nexus server launching is quite long process 
const std::chrono::milliseconds kHttpSendTimeout(20000);
const std::chrono::milliseconds kHttpReceiveTimeout(20000);

const std::chrono::milliseconds kKeepAliveTimeout(5000);
const std::chrono::milliseconds kMinNotificationInterval(500);
const std::chrono::milliseconds kMaxNotificationInterval(500);
const std::chrono::milliseconds kSetOutputStateTimeout(5000);

const QString kAlarmNameTemplate = lit("Alarm %1");
const QString kDigitalInputNameTemplate = lit("Digital Input %1");
const QString kDigitalOutputNameTemplate = lit("Digital Output %1");

} // namespace

using namespace nx::utils;

namespace nx{
namespace plugins{
namespace flir{

WebSocketIoManager::WebSocketIoManager(QnVirtualCameraResource* resource):
    m_resource(resource),
    m_initializationState(InitState::initial),
    m_nexusSessionId(kInvalidSessionId),
    m_monitoringIsInProgress(false),
    m_controlProxy(nullptr),
    m_notificationProxy(nullptr),
    m_isNexusServerEnabled(true),
    m_nexusServerHasJustBeenEnabled(false),
    m_nexusPort(kDefaultNexusPort)
{

}

WebSocketIoManager::~WebSocketIoManager()
{
    stopIOMonitoring();

    nx::utils::TimerId timerId = 0;
    {
        QnMutexLocker lock(&m_mutex);
        timerId = m_keepAliveTimerId;
    }

    if (timerId)
        TimerManager::instance()->joinAndDeleteTimer(timerId);
}

bool WebSocketIoManager::startIOMonitoring()
{
    qDebug() << "Flir, Starting IO monitoring";

    QnMutexLocker lock(&m_mutex);
    if (m_monitoringIsInProgress)
        return true;

    m_monitoringIsInProgress = true;

    QObject::disconnect();
    resetSocketProxies();

    m_controlProxy = new FlirWebSocketProxy();
    m_notificationProxy = new FlirWebSocketProxy();
    
    auto executorThread = FlirIoExecutor::instance()->getThread();
    executorThread->start();
    
    routeIOMonitoringInitialization(InitState::initial);

    return m_monitoringIsInProgress;
}

void WebSocketIoManager::stopIOMonitoring()
{
    qDebug() << "Flir, Stopping IO monitoring";

    QnMutexLocker lock(&m_mutex);
    if (!m_monitoringIsInProgress)
        return;

    m_monitoringIsInProgress = false;

    QObject::disconnect();
    resetSocketProxies();
}

bool WebSocketIoManager::setOutputPortState(const QString& portId, bool isActive)
{
    if (!m_monitoringIsInProgress)
        return false;

    QnMutexLocker lock(&m_mutex);
    if (m_initializationState < InitState::remoteControlObtained)
        return false;

    const QString kSetPortStateMessage = 
        lit("%1?session=%2&action=%3&Port=%4&Output=%5&State=%6")
            .arg(nexus::kCommandPrefix)
            .arg(m_nexusSessionId)
            .arg(nexus::kSetOutputPortStateCommand)
            .arg(getGpioModuleIdByPortId(portId))
            .arg(getPortNumberByPortId(portId))
            .arg(isActive ? 1 : 0);

    m_controlProxy->sendTextMessage(kSetPortStateMessage);

    //m_setOutputStateRequestCondition.wait(&m_mutex, kSetOutputStateTimeout.count());
    //Need to verify somehow 

    return true;
}

bool WebSocketIoManager::isMonitoringInProgress() const
{
    return m_monitoringIsInProgress;
}

QnIOPortDataList WebSocketIoManager::getInputPortList() const
{
    auto resData = getResourceData();
    auto allPorts = resData.value<QnIOPortDataList>(Qn::IO_SETTINGS_PARAM_NAME);
    QnIOPortDataList inputs;

    for (const auto& port : allPorts)
    {
        if (port.portType == Qn::IOPortType::PT_Input)
            inputs.push_back(port);
    }

    for (std::size_t i = 0; i < kMaxAlarmsNumber; ++i)
    {
        auto alarmInput = QnIOPortData();

        alarmInput.id = lit("%1:%2")
            .arg(nexus::kAlarmPrefix)
            .arg(i);

        alarmInput.inputName = kAlarmNameTemplate.arg(i);
        inputs.push_back(alarmInput);
    }

    QnMutexLocker lock(&m_mutex);
    for (auto& input : inputs)
    {
        input.portType = Qn::IOPortType::PT_Input;
        input.supportedPortTypes = Qn::IOPortType::PT_Input;
        input.iDefaultState = Qn::IODefaultState::IO_OpenCircuit; //< really?

        m_alarmStates[input.id] = false;
    }

    return inputs;
}

QnIOPortDataList WebSocketIoManager::getOutputPortList() const
{
    auto resData = getResourceData();
    auto allPorts = resData.value<QnIOPortDataList>(Qn::IO_SETTINGS_PARAM_NAME);
    QnIOPortDataList outputs;

    for (const auto& port : allPorts)
    {
        if (port.portType == Qn::IOPortType::PT_Output)
            outputs.push_back(port);
    }

    return outputs;
}

QnIOStateDataList WebSocketIoManager::getPortStates() const
{
    NX_ASSERT(false, lm("Unused. We should never be here."));
    return QnIOStateDataList();
}


nx_io_managment::IOPortState WebSocketIoManager::getPortDefaultState(const QString& /*portId*/) const
{
    NX_ASSERT(false, lm("Unused. We should never be here."));
    return nx_io_managment::IOPortState::nonActive;
}


void WebSocketIoManager::setPortDefaultState(
    const QString& /*portId*/,
    nx_io_managment::IOPortState/*state*/)
{
    NX_ASSERT(false, lm("Unused. We should never be here."));
}

void WebSocketIoManager::setInputPortStateChangeCallback(QnAbstractIOManager::InputStateChangeCallback callback)
{
    QnMutexLocker lock(&m_mutex);
    m_stateChangeCallback = callback;
}


void WebSocketIoManager::setNetworkIssueCallback(QnAbstractIOManager::NetworkIssueCallback callback)
{
    QnMutexLocker lock(&m_mutex);
    m_networkIssueCallback = callback;
}

void WebSocketIoManager::terminate()
{

}

//============================================================================

void WebSocketIoManager::routeIOMonitoringInitialization(InitState newState)
{
    if (!m_monitoringIsInProgress)
        return;

    qDebug() << "Routing IO Monitoring initialization" << (int)newState;
    m_initializationState = newState;

    switch (m_initializationState)
    {
        case InitState::initial:
            tryToGetNexusServerStatus();
            break;
        case InitState::nexusServerStatusRequested:
            tryToEnableNexusServer();
            break;
        case InitState::nexusServerEnabled:
        {
            std::chrono::milliseconds delay(0);
            if (m_nexusServerHasJustBeenEnabled)
                delay = std::chrono::milliseconds(5000);

            connectControlWebsocket(delay);
        }
        break;
        case InitState::controlSocketConnected:
            requestSessionId();
            break;
        case InitState::sessionIdObtained:
            requestRemoteControl();
            break;
        case InitState::remoteControlObtained:
            connectNotificationWebSocket();
            break;
        case InitState::subscribed:
            NX_LOG(lit("Flir, successfully subscribed to IO notifications"), cl_logDEBUG2);
            break;
        case InitState::error:
            NX_LOG(lit("Flir, error occurred when subscribing to IO notifications"), cl_logWARNING);
            break;
        default:
            NX_ASSERT(false, "We should never be here.");
    }
}

void WebSocketIoManager::at_controlWebSocketConnected()
{
    QnMutexLocker lock(&m_mutex);

    qDebug() << "Flir, control websocket connected";
    routeIOMonitoringInitialization(InitState::controlSocketConnected);
}

void WebSocketIoManager::at_controlWebSocketDisconnected()
{
    QnMutexLocker lock(&m_mutex);

    auto message = lm("Error occured, control websocket spuriously disconnected. Device %1 %2 (%3)")
        .arg(m_resource->getVendor())
        .arg(m_resource->getModel())
        .arg(m_resource->getUrl());

    if (m_monitoringIsInProgress)
    {
        qDebug() << message;
        NX_LOGX(message, cl_logWARNING);
        ;//< do reinit
    }
}

void WebSocketIoManager::at_controlWebSocketError(QAbstractSocket::SocketError error)
{
    QnMutexLocker lock(&m_mutex);

    auto message = lm("Flir, error occured on control web socket: %1. Device %2 %3 (%4). ")
        .arg(toString(error))
        .arg(m_resource->getVendor())
        .arg(m_resource->getModel())
        .arg(m_resource->getUrl());

    if (m_monitoringIsInProgress)
    {
        qDebug() << message;
        NX_LOGX(message, cl_logWARNING);
        ;//< do reinit
    }
}

void WebSocketIoManager::at_gotMessageOnControlSocket(const QString& message)
{
    using namespace nx::plugins::flir::nexus;

    Response response(message);

    if (!response.isValid())
        return;

    if (response.returnCode() != kNoError)
        return;

    const auto kResponseType = response.responseType();
    switch (kResponseType)
    {
        case Response::Type::serverWhoAmI:
            handleServerWhoAmIResponse(response);
            break;
        case Response::Type::serverRemoteControlRequest:
            handleRemoteControlRequestResponse(response);
            break;
        case Response::Type::serverRemoteControlRelease:
            handleRemoteControlReleaseResponse(response);
            break;
        case Response::Type::ioSensorOutputStateSet:
            handleIoSensorOutputStateSetResponse(response);
            break;
        default:
            NX_ASSERT(false, lit("We should never be here."));
    }
}

void WebSocketIoManager::at_notificationWebSocketConnected()
{
    QnMutexLocker lock(&m_mutex);
    qDebug() << "Flir, notification web socket connected";
    routeIOMonitoringInitialization(InitState::subscribed);
}

void WebSocketIoManager::at_notificationWebSocketDisconnected()
{
    QnMutexLocker lock(&m_mutex);

    auto message = lm("Error occured, notification websocket spuriously disconnected. Device %1 %2 (%3)")
        .arg(m_resource->getVendor())
        .arg(m_resource->getModel())
        .arg(m_resource->getUrl());

    if (m_monitoringIsInProgress)
    {
        qDebug() << message;
        NX_LOGX(message, cl_logWARNING);
        ;//< do reinit
    }
}

void WebSocketIoManager::at_notificationWebSocketError(QAbstractSocket::SocketError error)
{
    QnMutexLocker lock(&m_mutex);

    auto message = lm("Flir, error occured on control web socket: %1. Device %2 %3 (%4). ")
        .arg(toString(error))
        .arg(m_resource->getVendor())
        .arg(m_resource->getModel())
        .arg(m_resource->getUrl());

    if (m_monitoringIsInProgress)
    {
        qDebug() << message;
        NX_LOGX(message, cl_logWARNING);
        ;//< do reinit
    }
}

void WebSocketIoManager::connectWebsocket(
    const QString& path,
    FlirWebSocketProxy* proxy,
    std::chrono::milliseconds delay)
{
    auto doConnect = 
        [this, proxy, path] (nx::utils::TimerId timerId)
        {
            QnMutexLocker lock(&m_mutex);
            if (m_timerId != timerId)
                return;

            const auto kUrl = lit("ws://%1:%2/%3")
                .arg(getResourcesHostAddress())
                .arg(m_nexusPort)
                .arg(path);

            proxy->open(kUrl);

            qDebug() << "Flir, socket is opened";
        };

    m_timerId = TimerManager::instance()->addTimer(
        doConnect,
        delay);
}

void WebSocketIoManager::connectControlWebsocket(std::chrono::milliseconds delay)
{
    auto socket = m_controlProxy->getSocket();
    QObject::connect(
        socket, &QWebSocket::connected,
        this, &WebSocketIoManager::at_controlWebSocketConnected, Qt::QueuedConnection);

    QObject::connect(
        socket, &QWebSocket::disconnected,
        this, &WebSocketIoManager::at_controlWebSocketDisconnected, Qt::QueuedConnection);

    QObject::connect(
        socket, static_cast<ErrorSignalType>(&QWebSocket::error),
        this, &WebSocketIoManager::at_controlWebSocketError, Qt::QueuedConnection);

    QObject::connect(
        socket, &QWebSocket::textMessageReceived,
        this, &WebSocketIoManager::at_gotMessageOnControlSocket, Qt::QueuedConnection);


    const auto kControlPath = nexus::kControlPrefix;

    qDebug() << "Control socket's signals are connected";

    connectWebsocket(kControlPath, m_controlProxy, delay);
}

void WebSocketIoManager::connectNotificationWebSocket()
{
    auto socket = m_notificationProxy->getSocket();
    QObject::connect(
        socket, &QWebSocket::connected,
        this, &WebSocketIoManager::at_notificationWebSocketConnected, Qt::QueuedConnection);

    QObject::connect(
        socket, &QWebSocket::disconnected,
        this, &WebSocketIoManager::at_notificationWebSocketDisconnected, Qt::QueuedConnection);

    QObject::connect(
        socket, static_cast<ErrorSignalType>(&QWebSocket::error),
        this, &WebSocketIoManager::at_notificationWebSocketError, Qt::QueuedConnection);

    QObject::connect(
        socket, &QWebSocket::textMessageReceived,
        this, &WebSocketIoManager::handleNotification, Qt::QueuedConnection);

    const auto kNotificationPath = buildNotificationSubscriptionPath();

    qDebug() << "Notification socket's signals are connected, connecting to" << kNotificationPath;

    connectWebsocket(kNotificationPath, m_notificationProxy);
}

void WebSocketIoManager::tryToEnableNexusServer()
{
    QnMutexLocker lock(&m_mutex);
    if (!m_monitoringIsInProgress)
        return;

    auto onNexusServerResponseReceived =
        [this](nx_http::AsyncHttpClientPtr httpClient)
    {
        QnMutexLocker lock(&m_mutex);
        if (httpClient->state() != nx_http::AsyncHttpClient::sDone)
        {
            routeIOMonitoringInitialization(InitState::error);
            return;
        }

        m_isNexusServerEnabled = true;
        m_nexusServerHasJustBeenEnabled = true;
        routeIOMonitoringInitialization(InitState::nexusServerEnabled);
    };

    m_asyncHttpClient->doGet(
        lit("http://%1:%2%3")
        .arg(getResourcesHostAddress())
        .arg(nx_http::DEFAULT_HTTP_PORT)
        .arg(kStartNexusServerCommand),
        onNexusServerResponseReceived);
}

void WebSocketIoManager::tryToGetNexusServerStatus()
{
    QnMutexLocker lock(&m_mutex);
    if (!m_monitoringIsInProgress)
        return;

    auto onSettingsRequestDone =
        [this](nx_http::AsyncHttpClientPtr httpClient)
    {
        QnMutexLocker lock(&m_mutex);
        if (httpClient->state() != nx_http::AsyncHttpClient::sDone)
        {
            // It's ok, we can continue initialization using default Nexus port
            // and hoping that Nexus server is already enabled.
            routeIOMonitoringInitialization(InitState::nexusServerEnabled);
            return;
        }

        auto response = httpClient->response();
        if (!response)
        {
            routeIOMonitoringInitialization(InitState::nexusServerEnabled);
            return;
        }

        if (response->statusLine.statusCode != nx_http::StatusCode::ok)
        {
            routeIOMonitoringInitialization(InitState::nexusServerEnabled);
            return;
        }

        auto serverStatus = nexus::parseNexusServerStatusResponse(
            QString::fromUtf8(httpClient->fetchMessageBodyBuffer()));

        const auto& kSettings = serverStatus.settings;

        if (kSettings.find(kNexusInterfaceGroupName) != kSettings.cend())
        {
            const auto& group = kSettings.at(kNexusInterfaceGroupName);
            if (group.find(kNexusPortParamName) != group.cend())
            {
                bool status = true;
                const auto kNexusPort = group
                    .at(kNexusPortParamName)
                    .toInt(&status);

                if (status)
                    m_nexusPort = kNexusPort;
            }
        }

        if (serverStatus.isNexusServerEnabled)
            routeIOMonitoringInitialization(InitState::nexusServerEnabled);
        else
            routeIOMonitoringInitialization(InitState::nexusServerStatusRequested);
    };

    initHttpClient();
    m_asyncHttpClient->doGet(
        lit("http://%1:%2%3")
        .arg(getResourcesHostAddress())
        .arg(nx_http::DEFAULT_HTTP_PORT)
        .arg(kConfigurationFile),
        onSettingsRequestDone);
}

void WebSocketIoManager::requestSessionId()
{
    if (!m_monitoringIsInProgress)
        return;

    const auto kMessage = lit("%1?action=%2")
        .arg(nexus::kCommandPrefix)
        .arg(nexus::kServerWhoAmICommand);

    m_controlProxy->sendTextMessage(kMessage);
}

void WebSocketIoManager::requestRemoteControl()
{
    if (!m_monitoringIsInProgress)
        return;

    const int kForced = 1;
    const auto kMessage = lit("%1?session=%2&action=%3&Forced=%4")
        .arg(nexus::kCommandPrefix)
        .arg(m_nexusSessionId)
        .arg(nexus::kRequestControlCommand)
        .arg(kForced);

    m_controlProxy->sendTextMessage(kMessage);
}

QString WebSocketIoManager::buildNotificationSubscriptionPath() const
{
    nexus::SubscriptionStringBuilder builder;
    builder.setSessionId(m_nexusSessionId);
    builder.setNotificationFormat(nexus::kStringNotificationFormat);

    std::vector<nexus::Subscription> subscriptions = {
        nexus::Subscription(nexus::kAlarmSubscription)/*,
        nexus::Subscription(nexus::kThgSpotSubscription),
        nexus::Subscription(nexus::kThgAreaSubscription)*/
    };

    return builder.buildSubscriptionString(subscriptions);
}

void WebSocketIoManager::sendKeepAlive()
{
    if (!m_monitoringIsInProgress)
        return;

    auto keepAliveMessage = lit("%1?action=%2&%3=%4")
        .arg(nexus::kCommandPrefix)
        .arg(nexus::kServerWhoAmICommand)
        .arg(nexus::kSessionParamName)
        .arg(m_nexusSessionId);

    qDebug() << "Flir, Sending keep alive";
    m_controlProxy->sendTextMessage(keepAliveMessage);

    auto sendKeepAliveWrapper =
        [this](nx::utils::TimerId timerId)
        {
            QnMutexLocker lock(&m_mutex);
            if (timerId != m_keepAliveTimerId)
                return;

            sendKeepAlive();
        };

    m_keepAliveTimerId = TimerManager::instance()->addTimer(
        sendKeepAliveWrapper,
        kKeepAliveTimeout);
}

void WebSocketIoManager::handleNotification(const QString& message)
{
    bool status = true;
    auto notification = nexus::parseNotification(message, &status);

    if (!status)
        return;

    checkAndNotifyIfNeeded(notification);
}

void WebSocketIoManager::handleServerWhoAmIResponse(const nexus::Response& response)
{
    qDebug() << "Got session id response";

    const auto sessionId = response.value<int>(nexus::kSessionIdParamName);

    QnMutexLocker lock(&m_mutex);
    if (m_initializationState == InitState::controlSocketConnected)
    {
        m_keepAliveTimerId = TimerManager::instance()->addTimer(
            [this](TimerId timerId)
            {
                if (timerId != m_keepAliveTimerId)
                    return;

                sendKeepAlive();
            },
            kKeepAliveTimeout);

        m_nexusSessionId = sessionId;
        routeIOMonitoringInitialization(InitState::sessionIdObtained);
    }
}

void WebSocketIoManager::handleRemoteControlRequestResponse(const nexus::Response& response)
{
    QnMutexLocker lock(&m_mutex);
    if (m_initializationState != InitState::sessionIdObtained)
        return;

    if(!response.isValid())
        return;

    routeIOMonitoringInitialization(InitState::remoteControlObtained);
}

void WebSocketIoManager::handleRemoteControlReleaseResponse(const nexus::Response& /*response*/)
{
    qDebug() << "Got remote control release response. It's nice.";
}

void WebSocketIoManager::handleIoSensorOutputStateSetResponse(const nexus::Response& response)
{
    if (!response.isValid())
        return;

    m_setOutputStateRequestCondition.wakeOne();
}

void WebSocketIoManager::checkAndNotifyIfNeeded(const nexus::Notification& notification)
{
    QnMutexLocker lock(&m_mutex);
    if (!m_monitoringIsInProgress)
        return;

    const auto kAlarmName = notification.alarmId;

    if (m_alarmStates.find(kAlarmName) == m_alarmStates.end())
        return;

    if (m_alarmStates[kAlarmName] == notification.alarmState)
        return;

    m_alarmStates[kAlarmName] = notification.alarmState;

    lock.unlock(); //< Not sure if it is right
    m_stateChangeCallback(
        kAlarmName,
        nx_io_managment::fromBoolToIOPortState(notification.alarmState));
}

bool WebSocketIoManager::initHttpClient()
{
    auto auth = getResourceAuth();
    nx_http::AuthInfo authInfo;

    authInfo.username = auth.user();
    authInfo.password = auth.password();

    m_asyncHttpClient = nx_http::AsyncHttpClient::create();
    m_asyncHttpClient->setResponseReadTimeoutMs(kHttpReceiveTimeout.count());
    m_asyncHttpClient->setSendTimeoutMs(kHttpSendTimeout.count());
    m_asyncHttpClient->setAuth(authInfo);

    return true;
}

void WebSocketIoManager::resetSocketProxies()
{
    std::vector<FlirWebSocketProxy**> proxies = {
        &m_controlProxy,
        &m_notificationProxy};

    qDebug() << "Flir, Resetting socket proxies";
    for (auto proxyPtr: proxies)
    {
        if (!*proxyPtr)
            continue;

        QMetaObject::invokeMethod(
            *proxyPtr,
            "deleteLater",
            Qt::QueuedConnection);

        *proxyPtr = nullptr;
    }
}

int WebSocketIoManager::getPortNumberByPortId(const QString& portId) const
{
    auto typeAndNumber = portId.split(L':');
    NX_ASSERT(
        typeAndNumber.size() == 2,
        lm("Wrong port id format. Expected: $TYPE:NUMBER, given: %1")
            .arg(portId));

    if (typeAndNumber.size() != 2)
        return 0; //< There is no output with such a number.

    return typeAndNumber[1].toInt();
}

int WebSocketIoManager::getGpioModuleIdByPortId(const QString& portId) const
{
    //Change this function if there will be devices with more than one GPIO module.
    NX_ASSERT(
        portId.startsWith(nexus::kDigitalInputPrefix),
        lm("Only digital outputs belong to device's GPIO module. Given Id: %1")
            .arg(portId));
    
    return 0;
}

QString WebSocketIoManager::getResourcesHostAddress() const
{
    return m_resource->getHostAddress();
}

QAuthenticator WebSocketIoManager::getResourceAuth() const
{
    return m_resource->getAuth();
}

QnResourceData WebSocketIoManager::getResourceData() const
{
    return qnCommon->dataPool()->data(
        m_resource->getVendor(),
        m_resource->getModel());
}

} //namespace flir
} //namespace plugins
} //namespace nx