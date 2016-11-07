#include <chrono>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

#include "flir_ws_io_manager.h"
#include "flir_io_executor.h"

#include <core/resource/camera_resource.h>
#include <common/common_module.h>
#include <core/resource_management/resource_data_pool.h>
#include <nx/utils/log/log.h>

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

const quint16 kDefaultNexusPort = 8090;

const QString kCommandPrefix = lit("Nexus.cgi");
const QString kControlPrefix = lit("NexusWS.cgi");
const QString kSubscriptionPrefix = lit("NexusWS_Status.cgi");
const QString kServerWhoAmICommand = lit("SERVERWhoAmI");

const QString kSessionParamName = lit("session");
const QString kSubscriptionNumParamName = lit("numSubscriptions");
const QString kNotificationFormatParamName = lit("NotificationFormat");

const QString kJsonNotificationFormat = lit("JSON");
const QString kStringNotificationFormat = lit("String");

// Timeouts is so big because Nexus server launching is quite long process 
const std::chrono::milliseconds kHttpSendTimeout(20000);
const std::chrono::milliseconds kHttpReceiveTimeout(20000);

const std::chrono::milliseconds kKeepAliveTimeout(1000);
const std::chrono::milliseconds kMinNotificationInterval(500);
const std::chrono::milliseconds kMaxNotificationInterval(500);

const int kNoError = 0;
const int kAnyDevice = -1;

const QString kAlarmSubscription = lit("ALARM");
const QString kThgSpotSubscription = lit("THGSPOT");
const QString kThgAreaSubscription = lit("THGAREA");
const QString kIOSubscription = lit("IO");

const QString kAlarmPrefix = lit("$ALARM");
const QString kThgSpotPrefix = lit("$THGSPOT");
const QString kThgAreaPrefix = lit("$THGAREA");
const QString kIOPrefix = lit("$DI");

const int kMdDeviceType = 12;
const int kIODeviceType = 28;
const int kThgObjectDeviceType = 5;
const int kThgSpotDeviceType = 53;
const int kThgAreaDeviceType = 54;

const QString kDateTimeFormat("yyyyMMddhhmmsszzz");

} // namespace

using namespace nx::utils;

FlirWebSocketIoManager::FlirWebSocketIoManager(QnResource* resource):
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

FlirWebSocketIoManager::~FlirWebSocketIoManager()
{
    stopIOMonitoring();
}


bool FlirWebSocketIoManager::startIOMonitoring()
{
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


void FlirWebSocketIoManager::stopIOMonitoring()
{
    QnMutexLocker lock(&m_mutex);
    if (!m_monitoringIsInProgress)
        return;

    m_monitoringIsInProgress = false;
    resetSocketProxies();

    QObject::disconnect();
}

bool FlirWebSocketIoManager::setOutputPortState(const QString& portId, bool isActive)
{
    // Actually this function does nothing because all the outputs are handled
    // by the Onvif driver not this IO manager.
    return true;
}

bool FlirWebSocketIoManager::isMonitoringInProgress() const
{
    QnMutexLocker lock(&m_mutex);
    return m_monitoringIsInProgress;
}

QnIOPortDataList FlirWebSocketIoManager::getOutputPortList() const
{
    // Same as above - handled by Onvif.
    return QnIOPortDataList();
}

QnIOPortDataList FlirWebSocketIoManager::getInputPortList() const
{
    auto resData = getResourceData();
    auto inputs = resData.value<QnIOPortDataList>(Qn::IO_SETTINGS_PARAM_NAME);

    for (std::size_t i = 0; i < kMaxAlarmsNumber; ++i)
    {
        auto alarmInput = QnIOPortData();

        alarmInput.id = lit("%1:%2")
            .arg(kAlarmPrefix)
            .arg(i);

        alarmInput.inputName = lit("Alarm %1").arg(i);
        inputs.push_back(alarmInput);
    }

    QnMutexLocker lock(&m_mutex);
    for (auto& input: inputs)
    {
        input.portType = Qn::IOPortType::PT_Input;
        input.supportedPortTypes = Qn::IOPortType::PT_Input;
        input.iDefaultState = Qn::IODefaultState::IO_OpenCircuit; //< really?

        m_alarmStates[input.id] = false;
    }

    return inputs;
}

QnIOStateDataList FlirWebSocketIoManager::getPortStates() const
{
    // Unused.
    return QnIOStateDataList();
}


nx_io_managment::IOPortState FlirWebSocketIoManager::getPortDefaultState(const QString& /*portId*/) const
{
    // Unused.
    return nx_io_managment::IOPortState::nonActive;
}


void FlirWebSocketIoManager::setPortDefaultState(
    const QString& /*portId*/,
    nx_io_managment::IOPortState/*state*/)
{
    // Unused.
}


void FlirWebSocketIoManager::setInputPortStateChangeCallback(QnAbstractIOManager::InputStateChangeCallback callback)
{
    QnMutexLocker lock(&m_mutex);
    m_stateChangeCallback = callback;
}


void FlirWebSocketIoManager::setNetworkIssueCallback(QnAbstractIOManager::NetworkIssueCallback callback)
{
    QnMutexLocker lock(&m_mutex);
    m_networkIssueCallback = callback; 
}


void FlirWebSocketIoManager::terminate()
{
    directDisconnectAll();
}

void FlirWebSocketIoManager::at_controlWebSocketConnected()
{
    QnMutexLocker lock(&m_mutex);
    qDebug() << "Flir, control websocket connected";
    routeIOMonitoringInitialization(InitState::controlSocketConnected);
}

void FlirWebSocketIoManager::at_controlWebSocketDisconnected()
{
    QnMutexLocker lock(&m_mutex);
    //What should I do here ? 
    qDebug() << "Flir, control web socket disconnected";
}

void FlirWebSocketIoManager::at_controlWebSocketError(QAbstractSocket::SocketError error)
{
    QnMutexLocker lock(&m_mutex);
    qDebug() << "Flir, error occured on control web socket" << error;
}

void FlirWebSocketIoManager::at_notificationWebSocketConnected()
{
    QnMutexLocker lock(&m_mutex);
    qDebug() << "Flir, notification web socket disconnected";
    routeIOMonitoringInitialization(InitState::subscribed);
}

void FlirWebSocketIoManager::at_notificationWebSocketDisconnected()
{
    QnMutexLocker lock(&m_mutex);
    qDebug() << "Flir, notification web socket disconnected";
}

void FlirWebSocketIoManager::at_notificationWebSocketError(QAbstractSocket::SocketError error)
{
    QnMutexLocker lock(&m_mutex);
    qDebug() << "Flir, error occured on notification web socket" << error;
}

void FlirWebSocketIoManager::connectWebsocket(
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

            qDebug() << "Flir, Connecting websocket to url" << kUrl;

            proxy->open(kUrl);

            qDebug() << "Flir, socket is opened";
        };

    m_timerId = TimerManager::instance()->addTimer(
        doConnect,
        delay);
}

void FlirWebSocketIoManager::connectControlWebsocket(std::chrono::milliseconds delay)
{
    auto socket = m_controlProxy->getSocket();
    QObject::connect(
        socket, &QWebSocket::connected,
        this, &FlirWebSocketIoManager::at_controlWebSocketConnected, Qt::QueuedConnection);

    QObject::connect(
        socket, &QWebSocket::disconnected,
        this, &FlirWebSocketIoManager::at_controlWebSocketDisconnected, Qt::QueuedConnection);

    QObject::connect(
        socket, static_cast<ErrorSignalType>(&QWebSocket::error),
        this, &FlirWebSocketIoManager::at_controlWebSocketError, Qt::QueuedConnection);

    const auto kControlPath = kControlPrefix/* + kServerWhoAmICommand*/;

    qDebug() << "Control socket's signals are connected";

    connectWebsocket(kControlPath, m_controlProxy, delay);
}

void FlirWebSocketIoManager::connectNotificationWebSocket()
{
    auto socket = m_notificationProxy->getSocket();
    QObject::connect(
        socket, &QWebSocket::connected,
        this, &FlirWebSocketIoManager::at_notificationWebSocketConnected, Qt::QueuedConnection);

    QObject::connect(
        socket, &QWebSocket::disconnected,
        this, &FlirWebSocketIoManager::at_notificationWebSocketDisconnected, Qt::QueuedConnection);

    QObject::connect(
        socket, static_cast<ErrorSignalType>(&QWebSocket::error),
        this, &FlirWebSocketIoManager::at_notificationWebSocketError, Qt::QueuedConnection);

    QObject::connect(
        socket, &QWebSocket::textMessageReceived,
        this, &FlirWebSocketIoManager::handleNotification, Qt::QueuedConnection);

    const auto kNotificationPath = buildNotificationSubscriptionPath();

    qDebug() << "Notification socket's signals are connected, connecting to" << kNotificationPath;

    connectWebsocket(kNotificationPath, m_notificationProxy);
}

void FlirWebSocketIoManager::requestSessionId()
{
    if (!m_monitoringIsInProgress)
        return;

    const auto kMessage = lit("Nexus.cgi?action=") + kServerWhoAmICommand;

    auto sendKeepAliveWrapper =
        [this](nx::utils::TimerId timerId)
        {
            QnMutexLocker lock(&m_mutex);
            if (timerId != m_keepAliveTimerId)
                return;

            sendKeepAlive();
        };

    auto parseResponseAndScheduleKeepAlive = 
        [this, sendKeepAliveWrapper](const QString& message)
        {
            QnMutexLocker lock(&m_mutex);
            auto response = parseControlMessage(message);
            if (response.returnCode != kNoError)
            {
                qDebug() << "Flir, error occurred while requesting control token, code:" << response.returnCode;
            }

            m_nexusSessionId = response.sessionId;

            m_keepAliveTimerId = TimerManager::instance()->addTimer(
                sendKeepAliveWrapper,
                kKeepAliveTimeout);

            if (m_initializationState < InitState::sessionIdObtained)
                routeIOMonitoringInitialization(InitState::sessionIdObtained);
        };

    QObject::connect(
        m_controlProxy->getSocket(), &QWebSocket::textMessageReceived,
        this, parseResponseAndScheduleKeepAlive, Qt::QueuedConnection);
    
    m_controlProxy->sendTextMessage(kMessage);
}

FlirWebSocketIoManager::ServerWhoAmIResponse FlirWebSocketIoManager::parseControlMessage(const QString& message)
{
    auto jsonDocument = QJsonDocument::fromJson(message.toUtf8());
    auto jsonObject = jsonDocument.object();

    ServerWhoAmIResponse response;

    if (jsonObject.contains(kServerWhoAmICommand))
    {
        if (!jsonObject[kServerWhoAmICommand].isObject())
            return response;

        auto serverWhoAmI = jsonObject[kServerWhoAmICommand].toObject();

        if (serverWhoAmI.contains("Return Code"))
            response.returnCode = serverWhoAmI["Return Code"].toInt();

        if (serverWhoAmI.contains("Return String"))
            response.returnString = serverWhoAmI["Return String"].toString();

        if (serverWhoAmI.contains("Id"))
            response.sessionId = serverWhoAmI["Id"].toInt();

        if (serverWhoAmI.contains("Owner"))
            response.owner = serverWhoAmI["Owner"].toInt();

        if (serverWhoAmI.contains("ip"))
            response.ip = serverWhoAmI["ip"].toString();
    }

    qDebug() << "Got SERVERWHOAMI response:" 
        << "Return Code" << response.returnCode
        << "Return String" << response.returnString
        << "Session Id" << response.sessionId
        << "Owner" << response.owner
        << "IP" << response.ip;

    return response;
}

QString FlirWebSocketIoManager::buildNotificationSubscriptionPath() const
{
    QUrlQuery query;

    const int kSubscriptionNumber = 1;
    const std::vector<QString> kSubscriptions = {
        kAlarmSubscription,
        /*kThgSpotSubscription,
        kThgAreaSubscription*/};

    query.addQueryItem(kSessionParamName, QString::number(m_nexusSessionId));
    query.addQueryItem(kSubscriptionNumParamName, QString::number(kSubscriptions.size()));
    query.addQueryItem(kNotificationFormatParamName, kStringNotificationFormat);

    for (auto i = 0; i < kSubscriptions.size(); ++i)
    {
        query.addQueryItem(
            lit("subscription%1").arg(i + 1),
            buildNotificationSubscriptionParamString(kSubscriptions[i]));
    }

    return kSubscriptionPrefix + lit("?") + query.toString(QUrl::FullyEncoded);
}

QString FlirWebSocketIoManager::buildNotificationSubscriptionParamString(const QString& subscriptionType) const
{
    const int kPeriodicNotificationsMode = 0;

    auto subscriptionString = lit("\"%1,%2,%3,%4,%5\"")
        .arg(subscriptionType)
        .arg(kAnyDevice)
        .arg(kMinNotificationInterval.count())
        .arg(kMaxNotificationInterval.count())
        .arg(kPeriodicNotificationsMode);

    qDebug() << "Flir, building subscription string" << subscriptionString;

    return subscriptionString;
}

FlirWebSocketIoManager::AlarmEvent FlirWebSocketIoManager::parseNotification(
    const QString& message,
    bool* outStatus) const
{
    AlarmEvent notification;

    const auto kParts = message.split(L',');
    const auto kNotificationType = kParts[0];

    if (kNotificationType == kAlarmPrefix)
        notification = parseAlarmNotification(kParts, outStatus);
    else if (isThgObjectNotificationType(kNotificationType))
        notification = parseThgObjectNotification(kParts, outStatus);
    else
        *outStatus = false;

    return notification;
}

FlirWebSocketIoManager::AlarmEvent FlirWebSocketIoManager::parseThgObjectNotification(
    const QStringList& notificationParts,
    bool* outStatus) const
{
    AlarmEvent alarmEvent;

    const auto kObjectTypeFieldPosition = 0;
    const auto kObjectIndexFieldPosition = 7;
    const auto kObjectStateFieldPosition = 11;

    const auto kObjectType = notificationParts[kObjectTypeFieldPosition];
    const auto kObjectIndex = notificationParts[kObjectIndexFieldPosition].toInt(outStatus);

    if (!*outStatus)
        return alarmEvent;

    NX_ASSERT(
        isThgObjectNotificationType(kObjectType),
        lm("Flir notification parser: wrong notification type %1. %2 or %3 are expected.")
            .arg(kObjectType)
            .arg(kThgAreaPrefix)
            .arg(kThgSpotPrefix));

    if (!isThgObjectNotificationType(kObjectType))
    {
        *outStatus = false;
        return alarmEvent;
    }

    alarmEvent.alarmId = lit("%1:%2")
        .arg(kObjectType)
        .arg(kObjectIndex);

    alarmEvent.alarmState = notificationParts[kObjectStateFieldPosition].toInt(outStatus);

    return alarmEvent;
}

FlirWebSocketIoManager::AlarmEvent FlirWebSocketIoManager::parseAlarmNotification(
    const QStringList& notificationParts,
    bool* outStatus) const
{
    AlarmEvent alarmEvent;

    const auto kDeviceTypeFieldPosition = 5;
    const auto kAlarmSourceIndexFieldPosition = 6;
    const auto kAlarmStateFieldPosition = 9;

    const auto kDeviceType = notificationParts[kDeviceTypeFieldPosition]
        .toInt(outStatus);

    if (!*outStatus)
        return alarmEvent;

    const auto kAlarmSourceIndex = notificationParts[kAlarmSourceIndexFieldPosition]
        .toInt(outStatus);

    if (!*outStatus)
        return alarmEvent;

    QString prefix;
    if (kDeviceType == kIODeviceType)
        prefix = kIOPrefix;
    else if (kDeviceType == kThgObjectDeviceType)
        prefix = kAlarmPrefix;
        

    alarmEvent.alarmId = lit("%1:%2")
        .arg(prefix)
        .arg(kAlarmSourceIndex);

    alarmEvent.alarmState = notificationParts[kAlarmStateFieldPosition].toInt(outStatus);

    return alarmEvent;
}

bool FlirWebSocketIoManager::isThgObjectNotificationType(const QString& notificationType) const
{
    return notificationType == kThgSpotPrefix || notificationType == kThgAreaPrefix;
}

qint64 FlirWebSocketIoManager::parseFlirDateTime(const QString& dateTimeString, bool* outStatus)
{
    *outStatus = true;
    auto dateTime = QDateTime::fromString(dateTimeString, kDateTimeFormat);

    if (!dateTime.isValid())
        outStatus = false;

    return dateTime.toMSecsSinceEpoch();
}

void FlirWebSocketIoManager::sendKeepAlive()
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

            sendKeepAlive();
        };

    m_keepAliveTimerId = TimerManager::instance()->addTimer(
        sendKeepAliveWrapper,
        kKeepAliveTimeout);
}

void FlirWebSocketIoManager::handleNotification(const QString& message)
{
    bool status = true;
    auto notification = parseNotification(message, &status);

    if (!status)
        return;

    checkAndNotifyIfNeeded(notification);
}

void FlirWebSocketIoManager::checkAndNotifyIfNeeded(const AlarmEvent& notification)
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

void FlirWebSocketIoManager::tryToEnableNexusServer()
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

void FlirWebSocketIoManager::tryToGetNexusServerStatus()
{
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

            auto serverStatus = parseNexusServerStatusResponse(
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

bool FlirWebSocketIoManager::initHttpClient()
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

void FlirWebSocketIoManager::routeIOMonitoringInitialization(InitState newState)
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

QString FlirWebSocketIoManager::getResourcesHostAddress() const
{
    auto virtualResource = dynamic_cast<QnVirtualCameraResource*>(m_resource);
    NX_ASSERT(virtualResource, lit("Resource should be inherited from QnVirtualCameraResource"));
    if (!virtualResource)
        return QString();

    return virtualResource->getHostAddress();

    /*return lit("122.116.27.82");*/
}

QAuthenticator FlirWebSocketIoManager::getResourceAuth() const
{
    auto virtualResource = dynamic_cast<QnVirtualCameraResource*>(m_resource);
    NX_ASSERT(virtualResource, lit("Resource should be inherited from QnVirtualCameraResource"));
    if (!virtualResource)
        return QAuthenticator();

    return virtualResource->getAuth();

    /*QAuthenticator auth;

    auth.setUser(lit("admin"));
    auth.setPassword(lit("fliradmin"));

    return auth;*/
}

QnResourceData FlirWebSocketIoManager::getResourceData() const
{
    auto virtualResource = dynamic_cast<QnVirtualCameraResource*>(m_resource);
    NX_ASSERT(virtualResource, lit("Resource should be inherited from QnVirtualCameraResource"));
    if (!virtualResource)
        return QnResourceData();

    return qnCommon->dataPool()->data(
        virtualResource->getVendor(),
        virtualResource->getModel());

    /*return qnCommon->dataPool()->data(
        lit("FLIR"),
        lit("FC-632"));*/
}

FlirWebSocketIoManager::NexusServerStatus FlirWebSocketIoManager::parseNexusServerStatusResponse(
    const QString& response) const
{
    qDebug() << "Parsing Nexus server status response:" << response;
    NexusServerStatus serverStatus;
    QString currentGroupName;
    auto lines = response.split(lit("\n"));
    
    if (lines.isEmpty())
        return serverStatus;

    serverStatus.isNexusServerEnabled = lines.back().trimmed().toInt();
    lines.pop_back();

    for (const auto& line: lines)
    {
        auto trimmedLine  = line.trimmed();
        if (trimmedLine.startsWith(L'['))
        {
            // Group name pattern is [Group Name]
            currentGroupName = trimmedLine
                .mid(1, trimmedLine.size() - 2)
                .trimmed();
        }
        else
        {
            auto settingNameAndValue = trimmedLine.split("=");
            auto partsCount = settingNameAndValue.size();
            auto& currentGroup = serverStatus.settings[currentGroupName];

            if(partsCount == 2)
                currentGroup[settingNameAndValue[0]] = settingNameAndValue[1];
            else if (partsCount == 1)
                currentGroup[settingNameAndValue[0]] = QString();
        }
    }

    return serverStatus;
}


void FlirWebSocketIoManager::resetSocketProxies()
{
    std::vector<FlirWebSocketProxy**> proxies = {
        &m_controlProxy,
        &m_notificationProxy};

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
