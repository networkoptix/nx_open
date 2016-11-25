#include <chrono>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

#include "flir_web_socket_io_manager.h"
#include "flir_io_executor.h"
#include "flir_nexus_common.h"
#include "flir_parsing_utils.h"
#include "flir_nexus_string_builder.h"

#include <core/resource/camera_resource.h>
#include <common/common_module.h>
#include <core/resource_management/resource_data_pool.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/to_string.h>
#include <nx/network/http/httpclient.h>

namespace {
using ErrorSignalType = void(QWebSocket::*)(QAbstractSocket::SocketError);

const int kInvalidSessionId = -1;
const std::size_t kMaxThgAreasNumber = 4;
const std::size_t kMaxThgSpotsNumber = 8;
const std::size_t kMaxAlarmsNumber = 10;

const std::chrono::milliseconds kKeepAliveTimeout(5000);
const std::chrono::milliseconds kMinNotificationInterval(500);
const std::chrono::milliseconds kMaxNotificationInterval(500);
const std::chrono::milliseconds kSetOutputStateTimeout(5000);

const QString kAlarmNameTemplate = lit("Alarm %1");
const QString kDigitalInputNameTemplate = lit("Digital Input %1");
const QString kDigitalOutputNameTemplate = lit("Digital Output %1");

} // namespace

using namespace nx::utils;

namespace nx {
namespace plugins {
namespace flir {
namespace nexus {

WebSocketIoManager::WebSocketIoManager(QnVirtualCameraResource* resource, quint16 nexusPort):
    m_resource(resource),
    m_initializationState(InitState::initial),
    m_nexusSessionId(kInvalidSessionId),
    m_monitoringIsInProgress(false),
    m_controlProxy(nullptr),
    m_notificationProxy(nullptr),
    m_nexusPort(nexusPort)
{
    QnMutexLocker lock(&m_mutex);
    initIoPortStatesUnsafe();
}

WebSocketIoManager::~WebSocketIoManager()
{
    terminate();
}

bool WebSocketIoManager::startIOMonitoring()
{
    QnMutexLocker lock(&m_mutex);

    qDebug() << "Flir, Starting IO monitoring";
    if (m_monitoringIsInProgress)
        return true;

    m_monitoringIsInProgress = true;

    QObject::disconnect();
    resetSocketProxiesUnsafe();

    m_controlProxy = new WebSocketProxy();
    m_notificationProxy = new WebSocketProxy();
    
    auto executorThread = IoExecutor::instance()->getThread();
    executorThread->start();
    
    routeIOMonitoringInitializationUnsafe(InitState::initial);

    return m_monitoringIsInProgress;
}

void WebSocketIoManager::stopIOMonitoring()
{
    QnMutexLocker lock(&m_mutex);

    qDebug() << "Flir, Stopping IO monitoring";
    if (!m_monitoringIsInProgress)
        return;

    m_monitoringIsInProgress = false;

    QObject::disconnect();
    resetSocketProxiesUnsafe();
}

bool WebSocketIoManager::setOutputPortState(const QString& portId, bool isActive)
{
    QString path;
    QUrl url;

    {
        QnMutexLocker lock(&m_mutex);
        if (!m_monitoringIsInProgress)
            return false;

        if (m_initializationState < InitState::remoteControlObtained)
            return false;

        const QString path =
            lit("%1?session=%2&action=%3&Port=%4&Output=%5&State=%6")
            .arg(kCommandPrefix)
            .arg(m_nexusSessionId)
            .arg(kSetOutputPortStateCommand)
            .arg(getGpioModuleIdByPortId(portId))
            .arg(getPortNumberByPortId(portId))
            .arg(isActive ? 1 : 0);

        url = m_resource->getUrl();
        url.setPath(path);
    }

    nx_http::HttpClient httpClient;
    httpClient.setSendTimeoutMs(kSetOutputStateTimeout.count());
    httpClient.setResponseReadTimeoutMs(kSetOutputStateTimeout.count());
    httpClient.setMessageBodyReadTimeoutMs(kSetOutputStateTimeout.count());

    auto success = httpClient.doGet(url);
    
    if (!success)
        return false;

    auto response = httpClient.response();
    if (!response)
        return false;

    if (response->statusLine.statusCode != nx_http::StatusCode::ok)
        return false;

    nx_http::BufferType messageBody;
    while (!httpClient.eof())
        messageBody.append(httpClient.fetchMessageBodyBuffer());

    auto commandResponse = CommandResponse(QString::fromUtf8(messageBody));
    if (!commandResponse.isValid())
        return false;

    if (commandResponse.returnCode() != kNoError)
        return false;

    return true;
}

bool WebSocketIoManager::isMonitoringInProgress() const
{
    return m_monitoringIsInProgress;
}

QnIOPortDataList WebSocketIoManager::getInputPortList() const
{
    QnMutexLocker lock(&m_mutex);
    return m_inputs;
}

QnIOPortDataList WebSocketIoManager::getOutputPortList() const
{
    QnMutexLocker lock(&m_mutex);
    return m_outputs;
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
    stopIOMonitoring();

    nx::utils::TimerId timerId = 0;
    {
        QnMutexLocker lock(&m_mutex);
        timerId = m_keepAliveTimerId;
    }

    if (timerId)
        TimerManager::instance()->joinAndDeleteTimer(timerId);
}

//============================================================================

void WebSocketIoManager::routeIOMonitoringInitializationUnsafe(InitState newState)
{
    if (!m_monitoringIsInProgress)
        return;

    qDebug() << "Routing IO Monitoring initialization" << (int)newState;
    m_initializationState = newState;

    switch (m_initializationState)
    {
        case InitState::initial:
            connectControlWebsocketUnsafe(std::chrono::milliseconds::zero());
        break;
        case InitState::controlSocketConnected:
            requestSessionIdUnsafe();
            break;
        case InitState::sessionIdObtained:
            requestRemoteControlUnsafe();
            break;
        case InitState::remoteControlObtained:
            connectNotificationWebSocketUnsafe();
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
    
    auto message = lm("Control websocket has been connected. Device %1 %2 (%3)")
        .arg(m_resource->getVendor())
        .arg(m_resource->getModel())
        .arg(m_resource->getUrl());

    NX_LOGX(message, cl_logDEBUG2);
    qDebug() << message;
    routeIOMonitoringInitializationUnsafe(InitState::controlSocketConnected);
}

void WebSocketIoManager::at_controlWebSocketDisconnected()
{
    QnMutexLocker lock(&m_mutex);

    auto message = lm("Error occurred, control websocket spuriously disconnected. Device %1 %2 (%3)")
        .arg(m_resource->getVendor())
        .arg(m_resource->getModel())
        .arg(m_resource->getUrl());

    if (m_monitoringIsInProgress)
    {
        qDebug() << message;
        NX_LOGX(message, cl_logWARNING);
        reinitMonitoringUnsafe();
    }
}

void WebSocketIoManager::at_controlWebSocketError(QAbstractSocket::SocketError error)
{
    QnMutexLocker lock(&m_mutex);

    auto message = lm("Error occurred on control web socket: %1. Device %2 %3 (%4). ")
        .arg(toString(error))
        .arg(m_resource->getVendor())
        .arg(m_resource->getModel())
        .arg(m_resource->getUrl());

    if (m_monitoringIsInProgress)
    {
        qDebug() << message;
        NX_LOGX(message, cl_logWARNING);
        reinitMonitoringUnsafe();
    }
}

void WebSocketIoManager::at_gotMessageOnControlSocket(const QString& message)
{
    using namespace nx::plugins::flir::nexus;

    CommandResponse response(message);

    if (!response.isValid())
        return;

    if (response.returnCode() != kNoError)
        return;

    const auto kResponseType = response.responseType();

    QnMutexLocker lock(&m_mutex);
    switch (kResponseType)
    {
        case CommandResponse::Type::serverWhoAmI:
            handleServerWhoAmIResponseUnsafe(response);
            break;
        case CommandResponse::Type::serverRemoteControlRequest:
            handleRemoteControlRequestResponseUnsafe(response);
            break;
        case CommandResponse::Type::serverRemoteControlRelease:
            handleRemoteControlReleaseResponseUnsafe(response);
            break;
        case CommandResponse::Type::ioSensorOutputStateSet:
            handleIoSensorOutputStateSetResponseUnsafe(response);
            break;
        default:
            NX_ASSERT(false, lit("We should never be here."));
    }
}

void WebSocketIoManager::at_notificationWebSocketConnected()
{
    QnMutexLocker lock(&m_mutex);
    auto message = lm("Notification websocket has been connected. Device %1 %2 (%3)")
        .arg(m_resource->getVendor())
        .arg(m_resource->getModel())
        .arg(m_resource->getUrl());

    NX_LOGX(message, cl_logDEBUG2);
    qDebug() << message;

    routeIOMonitoringInitializationUnsafe(InitState::subscribed);
}

void WebSocketIoManager::at_notificationWebSocketDisconnected()
{
    QnMutexLocker lock(&m_mutex);

    auto message = lm("Error occurred, notification websocket spuriously disconnected. Device %1 %2 (%3)")
        .arg(m_resource->getVendor())
        .arg(m_resource->getModel())
        .arg(m_resource->getUrl());

    if (m_monitoringIsInProgress)
    {
        qDebug() << message;
        NX_LOGX(message, cl_logWARNING);
            reinitMonitoringUnsafe();
    }
}

void WebSocketIoManager::at_notificationWebSocketError(QAbstractSocket::SocketError error)
{
    QnMutexLocker lock(&m_mutex);

    auto message = lm("Error occurred on control web socket: %1. Device %2 %3 (%4). ")
        .arg(toString(error))
        .arg(m_resource->getVendor())
        .arg(m_resource->getModel())
        .arg(m_resource->getUrl());

    if (m_monitoringIsInProgress)
    {
        qDebug() << message;
        NX_LOGX(message, cl_logWARNING);
            reinitMonitoringUnsafe();
    }
}

void WebSocketIoManager::at_gotMessageOnNotificationWebSocket(const QString& message)
{
    qDebug() << "Got message from camera" << m_resource->getUrl() << message;
    auto notification = parseNotification(message);

    if (!notification)
        return;

    checkAndNotifyIfNeeded(notification.get());
}

void WebSocketIoManager::connectWebsocketUnsafe(
    const QString& path,
    WebSocketProxy* proxy,
    std::chrono::milliseconds delay)
{
    auto doConnect = 
        [this, proxy, path] (nx::utils::TimerId timerId)
        {
            QnMutexLocker lock(&m_mutex);
            if (m_timerId != timerId)
                return;

            const auto kUrl = lit("ws://%1:%2/%3")
                .arg(m_resource->getHostAddress())
                .arg(m_nexusPort)
                .arg(path);

            auto message = lm("Connecting socket to url").arg(kUrl);
            qDebug() << message;
            NX_LOGX(message, cl_logDEBUG2);

            proxy->open(kUrl);
        };

    m_timerId = TimerManager::instance()->addTimer(
        doConnect,
        delay);
}

void WebSocketIoManager::connectControlWebsocketUnsafe(std::chrono::milliseconds delay)
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

    connectWebsocketUnsafe(kControlPrefix, m_controlProxy, delay);
}

void WebSocketIoManager::connectNotificationWebSocketUnsafe()
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
        this, &WebSocketIoManager::at_gotMessageOnNotificationWebSocket, Qt::QueuedConnection);

    const auto kNotificationPath = buildNotificationSubscriptionPath();
    connectWebsocketUnsafe(kNotificationPath, m_notificationProxy);
}

void WebSocketIoManager::requestSessionIdUnsafe()
{
    if (!m_monitoringIsInProgress)
        return;

    const auto kMessage = lit("%1?action=%2")
        .arg(kCommandPrefix)
        .arg(kServerWhoAmICommand);

    m_controlProxy->sendTextMessage(kMessage);
}

void WebSocketIoManager::requestRemoteControlUnsafe()
{
    if (!m_monitoringIsInProgress)
        return;

    const int kForced = 1;
    const auto kMessage = lit("%1?session=%2&action=%3&Forced=%4")
        .arg(kCommandPrefix)
        .arg(m_nexusSessionId)
        .arg(kRequestControlCommand)
        .arg(kForced);

    m_controlProxy->sendTextMessage(kMessage);
}

QString WebSocketIoManager::buildNotificationSubscriptionPath() const
{
    SubscriptionStringBuilder builder;
    builder.setSessionId(m_nexusSessionId);
    builder.setNotificationFormat(kStringNotificationFormat);

    std::vector<Subscription> subscriptions = {
        Subscription(kAlarmSubscription)/*,
        Subscription(kThgSpotSubscription),
        Subscription(kThgAreaSubscription)*/
    };

    return builder.buildSubscriptionString(subscriptions);
}

void WebSocketIoManager::sendKeepAliveUnsafe()
{
    if (!m_monitoringIsInProgress)
        return;

    auto keepAliveMessage = lit("%1?action=%2&%3=%4")
        .arg(kCommandPrefix)
        .arg(kServerWhoAmICommand)
        .arg(kSessionParamName)
        .arg(m_nexusSessionId);

    m_controlProxy->sendTextMessage(keepAliveMessage);

    auto sendKeepAliveWrapper =
        [this](nx::utils::TimerId timerId)
        {
            QnMutexLocker lock(&m_mutex);
            if (timerId != m_keepAliveTimerId)
                return;

            sendKeepAliveUnsafe();
        };

    m_keepAliveTimerId = TimerManager::instance()->addTimer(
        sendKeepAliveWrapper,
        kKeepAliveTimeout);
}


void WebSocketIoManager::handleServerWhoAmIResponseUnsafe(const CommandResponse& response)
{
    qDebug() << "Got session id response";

    const auto sessionId = response.value<int>(kSessionIdParamName);
    if (!sessionId)
        return;

    if (m_initializationState == InitState::controlSocketConnected)
    {
        m_keepAliveTimerId = TimerManager::instance()->addTimer(
            [this](TimerId timerId)
            {
                QnMutexLocker lock(&m_mutex);
                if (timerId != m_keepAliveTimerId)
                    return;

                sendKeepAliveUnsafe();
            },
            kKeepAliveTimeout);

        m_nexusSessionId = sessionId.get();
        routeIOMonitoringInitializationUnsafe(InitState::sessionIdObtained);
    }
}

void WebSocketIoManager::handleRemoteControlRequestResponseUnsafe(const CommandResponse& response)
{
    if (m_initializationState != InitState::sessionIdObtained)
        return;

    if (!response.isValid())
        return;

    routeIOMonitoringInitializationUnsafe(InitState::remoteControlObtained);
}

void WebSocketIoManager::handleRemoteControlReleaseResponseUnsafe(const CommandResponse& /*response*/)
{
    qDebug() << "Got remote control release response. It's nice.";
}

void WebSocketIoManager::handleIoSensorOutputStateSetResponseUnsafe(const CommandResponse& response)
{
    
}

void WebSocketIoManager::checkAndNotifyIfNeeded(const Notification& notification)
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
    auto callback = m_stateChangeCallback;

    lock.unlock(); //< Not sure if it is right

    callback(
        kAlarmName,
        nx_io_managment::fromBoolToIOPortState(notification.alarmState));
}

void WebSocketIoManager::initIoPortStatesUnsafe()
{
    auto resData = qnCommon->dataPool()->data(
        m_resource->getVendor(),
        m_resource->getModel());

    auto allPorts = resData.value<QnIOPortDataList>(Qn::IO_SETTINGS_PARAM_NAME);

    for (const auto& port : allPorts)
    {
        if (port.portType == Qn::IOPortType::PT_Output)
        {
            m_outputs.push_back(port);
        }
        else if (port.portType == Qn::IOPortType::PT_Input)
        {
            m_inputs.push_back(port);
        }
    }

    for (std::size_t i = 0; i < kMaxAlarmsNumber; ++i)
    {
        auto alarmInput = QnIOPortData();

        alarmInput.id = lit("%1:%2")
            .arg(kAlarmPrefix)
            .arg(i);

        alarmInput.inputName = kAlarmNameTemplate.arg(i);
        m_inputs.push_back(alarmInput);
    }

    for (auto& input : m_inputs)
    {
        input.portType = Qn::IOPortType::PT_Input;
        input.supportedPortTypes = Qn::IOPortType::PT_Input;
        input.iDefaultState = Qn::IODefaultState::IO_OpenCircuit; //< really?

        m_alarmStates[input.id] = 0;
    }
}

void WebSocketIoManager::resetSocketProxiesUnsafe()
{
    std::vector<WebSocketProxy**> proxies = {
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

void WebSocketIoManager::reinitMonitoringUnsafe()
{
    auto message = lm("Something went wrong. Reinitializing IO monitoring. Device")
        .arg(m_resource->getVendor())
        .arg(m_resource->getModel())
        .arg(m_resource->getUrl());

    NX_LOGX(message, cl_logWARNING);
    qDebug() << message;

    QObject::disconnect();
    resetSocketProxiesUnsafe();
    m_controlProxy = new WebSocketProxy();
    m_notificationProxy = new WebSocketProxy();
    if (m_keepAliveTimerId)
        TimerManager::instance()->joinAndDeleteTimer(m_keepAliveTimerId);

    routeIOMonitoringInitializationUnsafe(InitState::initial);
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
        portId.startsWith(kDigitalInputPrefix),
        lm("Only digital outputs belong to device GPIO module. Given Id: %1")
            .arg(portId));
    
    return 0;
}

} // namespace nexus
} // namespace flir
} // namespace plugins
} // namespace nx