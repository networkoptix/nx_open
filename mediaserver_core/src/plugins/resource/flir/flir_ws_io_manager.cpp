#include <chrono>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

#include "flir_ws_io_manager.h"

#include <core/resource/camera_resource.h>
#include <common/common_module.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_data_pool.h>

namespace {
    using ErrorSignalType = void(QWebSocket::*)(QAbstractSocket::SocketError);

    //This group of settings is from a private API. We shouldn't rely on it.
    const QString kConfigurationFile("/api/server/status/full");
    const QString kStartNexusServerCommand("/api/server/start");
    const QString kStopNexusServerCommand("/api/server/stop");
    const QString kServerSuccessfullyStarted("1");
    const QString kServerSuccessfullyStopped("0");
    const QString kNumberOfInputsParamName("Number of IOs");
    const QString kNexusPortParamName("Port");

    const int kInvalidSessionId = -1;
    const std::size_t kMaxThgAreasNumber = 4;
    const std::size_t kMaxThgSpotsNumber = 8;

    //TODO: #dmishin don't forget to change it to 8090
    const quint16 kDefaultNexusPort = 8081;

    const QString kControlPrefix = lit("Nexus.cgi?action=");
    const QString kSubscriptionPrefix = lit("NexusWS_Status.cgi?");
    const QString kServerWhoAmICommand = lit("SERVERWhoAmI");

    const QString kSessionParamName = lit("session");
    const QString kSubscriptionNumParamName = lit("numSubscriptions");
    const QString kNotificationFormatParamName = lit("NotificationFormat");

    const QString kJsonNotificationFormat = lit("JSON");
    const QString kStringNotificationFormat = lit("String");

    const std::chrono::milliseconds kHttpSendTimeout(4000);
    const std::chrono::milliseconds kHttpReceivetimeout(4000);

    const std::chrono::milliseconds kKeepAliveTimeout(10000);
    const std::chrono::milliseconds kMinNotificationInterval(500);
    const std::chrono::milliseconds kMaxNotificationInterval(500);

    const int kNoError = 0;
    const int kAnyDevice = -1;
    const QString kAlarmSubscription = lit("ALARM");
    const QString kIOSubscription = lit("IO");
    const QString kThgSpotSubscription = lit("THGSPOT");
    const QString kThgAreaSubscription = lit("THGAREA");

    const QString kThgSpotPrefix = lit("thgSpot");
    const QString kThgAreaPrefix = lit("thgArea");
    const QString kIOPrefix = lit("di");

    const int kMdDeviceType = 12;
    const int kIODeviceType = 28;
    const int kThgSpotDeviceType = 53;
    const int kThgAreaDeviceType = 54;

    const QString kDateTimeFormat("yyyyMMddhhmmsszzz");

} //< anonymous namespace

using namespace nx::utils;

FlirWsIOManager::FlirWsIOManager(QnResource* resource):
    m_resource(resource),
    m_initializationState(InitState::initial),
    m_nexusSessionId(kInvalidSessionId),
    m_monitoringIsInProgress(false),
    m_controlWebSocket(new QWebSocket()),
    m_notificationWebSocket(new QWebSocket()),
    m_isNexusServerEnabled(false),
    m_nexusPort(kDefaultNexusPort)
{

}


FlirWsIOManager::~FlirWsIOManager()
{
    stopIOMonitoring();
    directDisconnectAll();
}


bool FlirWsIOManager::startIOMonitoring()
{
    //TODO: #dmsihin need mutex here (and everywhere else)
    if (m_monitoringIsInProgress)
        return true;

    m_monitoringIsInProgress = true;
    routeIOMonitoringInitialization(InitState::initial);

    return m_monitoringIsInProgress;
}


void FlirWsIOManager::stopIOMonitoring()
{
    if (!m_monitoringIsInProgress)
        return;

    m_monitoringIsInProgress = false;
}


bool FlirWsIOManager::setOutputPortState(const QString& portId, bool isActive)
{
    // Actually this function does nothing because all the outputs are handled
    // by the onvif driver not this IO manager.
    return true;
}


bool FlirWsIOManager::isMonitoringInProgress() const
{
    return m_monitoringIsInProgress;
}


QnIOPortDataList FlirWsIOManager::getOutputPortList() const
{
    // Same as above - handled by Onvif.
    return QnIOPortDataList();
}


QnIOPortDataList FlirWsIOManager::getInputPortList() const
{
    auto virtualResource = dynamic_cast<QnVirtualCameraResource*>(m_resource);
    NX_ASSERT(virtualResource, lit("Resource should be inherited from QnVirtualCameraResource"));
    if (!virtualResource)
        return QnIOPortDataList();

    auto resData = qnCommon->dataPool()->data(
        virtualResource->getVendor(),
        virtualResource->getModel());

    auto hasNoInputs = resData.value<bool>("noInputs"); //< TODO: #dmsihin add constant to params.h
    auto inputs = resData.value<QnIOPortDataList>(Qn::IO_SETTINGS_PARAM_NAME);

    if (!inputs.empty() || hasNoInputs)
        return inputs;

    for (std::size_t i = 0; i < kMaxThgAreasNumber; ++i)
    {
        auto thgAreaInput = QnIOPortData();

        thgAreaInput.id = lit("%1:%2")
            .arg(kThgAreaPrefix)
            .arg(i);

        thgAreaInput.inputName = lit("THG Area %1").arg(i);
        inputs.push_back(thgAreaInput);
    }

    for (std::size_t i = 0; i < kMaxThgSpotsNumber; ++i)
    {
        auto thgSpotInput = QnIOPortData();

        thgSpotInput.id = lit("%1:%2")
            .arg(kThgSpotPrefix)
            .arg(i);

        thgSpotInput.inputName = lit("THG Spot %1").arg(i);
        inputs.push_back(thgSpotInput);
    }

    for (auto& input: inputs)
    {
        input.portType = Qn::IOPortType::PT_Input;
        input.supportedPortTypes = Qn::IOPortType::PT_Input;
        input.iDefaultState = Qn::IODefaultState::IO_OpenCircuit; //< really?

        m_alarmStates[input.inputName] = false;
    }

    return inputs;
}


QnIOStateDataList FlirWsIOManager::getPortStates() const
{
    qDebug() << "Flir, calling getIOPortStates()";
    return QnIOStateDataList();
}


nx_io_managment::IOPortState FlirWsIOManager::getPortDefaultState(const QString& portId) const
{
    return nx_io_managment::IOPortState::nonActive;
}


void FlirWsIOManager::setPortDefaultState(const QString& portId, nx_io_managment::IOPortState state)
{
    // Do nothing.
}


void FlirWsIOManager::setInputPortStateChangeCallback(QnAbstractIOManager::InputStateChangeCallback callback)
{
    QnMutexLocker lock(&m_mutex);
    m_stateChangeCallback = callback;
}


void FlirWsIOManager::setNetworkIssueCallback(QnAbstractIOManager::NetworkIssueCallback callback)
{
    QnMutexLocker lock(&m_mutex);
    m_networkIssueCallback = callback; 
}


void FlirWsIOManager::terminate()
{
    directDisconnectAll();
}

void FlirWsIOManager::at_controlWebSocketConnected()
{
    qDebug() << "Flir, control websocket connected";
    routeIOMonitoringInitialization(InitState::controlSocketConnected);
}

void FlirWsIOManager::at_controlWebSocketDisconnected()
{
    //What should I do here ? 
    qDebug() << "Flir, control web socket disconnected";
}

void FlirWsIOManager::at_controlWebSocketError(QAbstractSocket::SocketError error)
{
    qDebug() << "Flir, error occured on control web socket" << error;
}

void FlirWsIOManager::at_notificationWebSocketConnected()
{
    qDebug() << "Flir, notification web socket disconnected";
    routeIOMonitoringInitialization(InitState::notificationSocketConnected);
}

void FlirWsIOManager::at_notificationWebSocketDisconnected()
{
    qDebug() << "Flir, notification web socket disconnected";
}

void FlirWsIOManager::at_notificationWebSocketError(QAbstractSocket::SocketError error)
{
    qDebug() << "Flir, error occured on notification web socket" << error;
}

void FlirWsIOManager::connectWebsocket(const QString& path, QWebSocket* socket)
{
    auto virtualResource = dynamic_cast<QnVirtualCameraResource*>(m_resource);
    NX_ASSERT(virtualResource, lit("Resource should be inherited from QnVirtualCameraResource"));
    if (!virtualResource)
        return;

    const auto kUrl = lit("ws://%1:%2/%3")
        .arg(virtualResource->getHostAddress())
        .arg(m_nexusPort)
        .arg(path);

    qDebug() << "Flir, Connecting websocket to url" << kUrl;

    socket->open(kUrl);
}

void FlirWsIOManager::connectControlWebsocket()
{
    Qn::directConnect(
        m_controlWebSocket.get(), &QWebSocket::connected,
        this, &FlirWsIOManager::at_controlWebSocketConnected);

    Qn::directConnect(
        m_controlWebSocket.get(), &QWebSocket::disconnected,
        this, &FlirWsIOManager::at_controlWebSocketDisconnected);

    Qn::directConnect(
        m_controlWebSocket.get(), static_cast<ErrorSignalType>(&QWebSocket::error),
        this, &FlirWsIOManager::at_controlWebSocketError);

    const auto kControlPath = kControlPrefix + kServerWhoAmICommand;

    connectWebsocket(kControlPath, m_controlWebSocket.get());
}

void FlirWsIOManager::connectNotificationWebSocket()
{
    Qn::directConnect(
        m_notificationWebSocket.get(), &QWebSocket::connected,
        this, &FlirWsIOManager::at_notificationWebSocketConnected);

    Qn::directConnect(
        m_notificationWebSocket.get(), &QWebSocket::disconnected,
        this, &FlirWsIOManager::at_notificationWebSocketDisconnected);

    Qn::directConnect(
        m_notificationWebSocket.get(), static_cast<ErrorSignalType>(&QWebSocket::error),
        this, &FlirWsIOManager::at_notificationWebSocketError);

    const auto kNotificationPath = buildNotificationSubscriptionPath();

    connectWebsocket(kNotificationPath, m_notificationWebSocket.get());
}

void FlirWsIOManager::requestSessionId()
{
    if (!m_monitoringIsInProgress)
        return;

    const auto kMessage = kControlPrefix + kServerWhoAmICommand;

    auto sendKeepAliveWrapper =
        [this](nx::utils::TimerId timerId)
        {
            if (timerId != m_timerId)
                return;

            sendKeepAlive();
        };

    auto parseResponseAndScheduleKeepAlive = 
        [this, sendKeepAliveWrapper](const QString& message)
        {
            auto response = parseControlMessage(message);
            if (response.returnCode != kNoError)
            {
                qDebug() << "Flir, error occurred while requesting control token, code:" << response.returnCode;
            }

            m_nexusSessionId = response.sessionId;

            TimerManager::instance()->addTimer(
                sendKeepAliveWrapper,
                kKeepAliveTimeout);

            routeIOMonitoringInitialization(InitState::sessionIdObtained);
        };

    Qn::directConnect(
        m_controlWebSocket.get(), &QWebSocket::textMessageReceived,
        this, parseResponseAndScheduleKeepAlive); //< Am not sure this is right. Looks like it's required to use member functions

    //TODO: #dmishin check number of sent bytes;
    auto bytesSent = m_controlWebSocket->sendTextMessage(kMessage);
}

FlirWsIOManager::FlirServerWhoAmIResponse FlirWsIOManager::parseControlMessage(const QString& message)
{
    auto jsonDocument = QJsonDocument::fromJson(message.toUtf8());
    auto jsonObject = jsonDocument.object();

    FlirServerWhoAmIResponse response;

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

QString FlirWsIOManager::buildNotificationSubscriptionPath() const
{
    QUrlQuery query;

    const int kSubscriptionNumber = 1;

    query.addQueryItem(kSessionParamName, QString::number(m_nexusSessionId));
    query.addQueryItem(kSubscriptionNumParamName, QString::number(kSubscriptionNumber));
    query.addQueryItem(kNotificationFormatParamName, kStringNotificationFormat);
    query.addQueryItem(lit("subscription1"), buildNotificationSubscriptionParamString(kAlarmSubscription));

    return kSubscriptionPrefix + query.toString();
}

QString FlirWsIOManager::buildNotificationSubscriptionParamString(const QString& subscriptionType) const
{
    const int kPeriodicNotificationsMode = 0;

    auto subscriptionString = lit("%1,%2,%3,%4,%5")
        .arg(subscriptionType)
        .arg(kAnyDevice)
        .arg(kMinNotificationInterval.count())
        .arg(kMaxNotificationInterval.count())
        .arg(kPeriodicNotificationsMode);

    qDebug() << "Flir, building subscription string" << subscriptionString;

    return subscriptionString;
}

void FlirWsIOManager::subscribeToNotifications()
{
    QUrlQuery query;

    const int kSubscriptionNumber = 1;

    query.addQueryItem(kSessionParamName, QString::number(m_nexusSessionId));
    query.addQueryItem(kSubscriptionNumParamName, QString::number(kSubscriptionNumber));
    query.addQueryItem(kNotificationFormatParamName, kStringNotificationFormat);
    query.addQueryItem(lit("subscription1"), buildNotificationSubscriptionParamString(kAlarmSubscription));

    const auto kSubscriptionMessage = kSubscriptionPrefix + query.toString();

    //TODO: #dmishin check number of sent bytes
    auto bytesSent = m_notificationWebSocket->sendTextMessage(kSubscriptionMessage);

    routeIOMonitoringInitialization(InitState::subscribed);    
}

FlirWsIOManager::FlirAlarmNotification FlirWsIOManager::parseNotification(const QString& message, bool* outStatus)
{
    FlirAlarmNotification notification;
    auto parts = message.split(L',');

    const std::size_t kPartsCount = 14;
    NX_ASSERT(
        parts.size() == kPartsCount,
        lm("Number of fields is %1, expected %2")
        .arg(parts.size())
        .arg(kPartsCount));

    bool status = true;

    //TODO: #dmishin check outStatus after each field parsing.
    notification.deviceId = parts[1].toInt(outStatus);
    notification.health = parts[2].toInt(outStatus);
    notification.lastBIT = parts[3].toInt(outStatus);
    notification.timestamp = parseFlirDateTime(parts[4], outStatus);
    notification.deviceType = parts[5].toInt(outStatus);
    notification.sourceIndex = parts[6].toInt(outStatus);
    notification.sourceName = parts[7];
    notification.timestamp2 = parts[8];
    notification.state = parts[9].toInt(outStatus);
    notification.latitude = parts[10].toFloat(outStatus);
    notification.longitude = parts[11].toFloat(outStatus);
    notification.altitude = parts[12].toFloat(outStatus);
    notification.autoacknowledge = parts[11].toInt(outStatus);

    return notification;
}

qint64 FlirWsIOManager::parseFlirDateTime(const QString& dateTimeString, bool* outStatus)
{
    *outStatus = true;
    auto dateTime = QDateTime::fromString(dateTimeString, kDateTimeFormat);

    if (!dateTime.isValid())
        outStatus = false;

    return dateTime.toMSecsSinceEpoch();
}

void FlirWsIOManager::sendKeepAlive()
{
    if (!m_monitoringIsInProgress)
        return;

    qDebug() << "Flir, sending keep alive";

    auto keepAliveMessage = lit("%1%2&%3=%4")
        .arg(kControlPrefix)
        .arg(kServerWhoAmICommand)
        .arg(kSessionParamName)
        .arg(m_nexusSessionId);

    auto bytesSent = m_controlWebSocket->sendTextMessage(keepAliveMessage);
    //TODO: #dmishin check number of sent bytes;

    auto sendKeepAliveWrapper =
        [this](nx::utils::TimerId timerId)
        {
            if (timerId != m_timerId)
                return;

            sendKeepAlive();
        };

    m_timerId = TimerManager::instance()->addTimer(
        sendKeepAliveWrapper,
        kKeepAliveTimeout);
}

void FlirWsIOManager::handleNotification(const QString& message)
{
    bool status = true;
    auto notification = parseNotification(message, &status);

    // TODO: #dmsihin maybe we should do smth 
    // if message wasn't successfully parsed.
    if (!status)
        return;

    checkAndNotifyIfNeeded(notification);
}

void FlirWsIOManager::checkAndNotifyIfNeeded(const FlirAlarmNotification& notification)
{
    QString deviceType;

    switch (notification.deviceType)
    {
        case kIODeviceType:
            deviceType += kIOPrefix;
            break;
        case kThgAreaDeviceType:
            deviceType += kThgAreaPrefix;
            break;
        case kThgSpotDeviceType:
            deviceType += kThgSpotPrefix;
            break;
        default:
            qDebug() << "Flir, notification with unknown device type" << notification.deviceType;
            return;
    }

    const auto kAlarmName = lit("%1:%2")
        .arg(deviceType)
        .arg(notification.sourceIndex);

    if (m_alarmStates.find(kAlarmName) == m_alarmStates.end())
        return;

    if (m_alarmStates[kAlarmName] ==  notification.state)
        return;

    m_stateChangeCallback(
        kAlarmName,
        nx_io_managment::fromBoolToIOPortState(notification.state));
}

bool FlirWsIOManager::tryToEnableNexusServer()
{
    /*auto onNexusServerResponseReceived = 
        [this](nx_http::AsyncHttpClientPtr httpClient)
        {
            if (httpClient->state() != nx_http::AsyncHttpClient::sDone)
            {
                qDebug() << "Flir, could not enable Nexus server";
                routeIOMonitoringInitialization(InitState::error);
                return;
            }

            m_isNexusServerEnabled = true;
            routeIOMonitoringInitialization(InitState::nexusServerEnabled);
        };

    m_asyncHttpClient->doGet(kStartNexusServerCommand, onNexusServerResponseReceived);*/

    routeIOMonitoringInitialization(InitState::nexusServerEnabled);
    return true;
}

bool FlirWsIOManager::tryToGetNexusSettings()
{
    auto onSettingsRequestDone = 
        [this](nx_http::AsyncHttpClientPtr httpClient)
        {
            qDebug() << "Got settings!";

            //TODO: #dmishin do something in case of error
            if (httpClient->state() != nx_http::AsyncHttpClient::sDone)
            {
                routeIOMonitoringInitialization(InitState::error);
                return;
            }

            auto response = httpClient->response();
            if (!response)
            {
                qDebug() << "Flir, No response received";
                routeIOMonitoringInitialization(InitState::error);
                return;
            }

            //TODO: #dmishin same as above.
            if (response->statusLine.statusCode != nx_http::StatusCode::ok)
            {
                qDebug() << "Flir, request wasn't successful" << response->statusLine.statusCode;
                routeIOMonitoringInitialization(InitState::error);
                return;
            }

            auto settings = QString::fromUtf8(response->messageBody);
            for (const auto& line: settings.split(lit("\r\n")))
            {
                if (line.startsWith(L'['))
                    continue;
                
                auto paramAndValue = line.split(L'=');
                if (paramAndValue.size() != 2)
                    continue;

                if (paramAndValue[0] == kNexusPortParamName)
                    m_nexusPort = paramAndValue[1].trimmed().toInt();

                if (paramAndValue[0] == kNumberOfInputsParamName)
                    ;//< Do something with this info
            }
            routeIOMonitoringInitialization(InitState::settingsRequested);
        };

    m_asyncHttpClient->doGet(kStartNexusServerCommand, onSettingsRequestDone);

    return true; //< totally useless 
}

bool FlirWsIOManager::initHttpClient()
{
    auto virtualResource = dynamic_cast<QnVirtualCameraResource*>(m_resource);

    NX_ASSERT(virtualResource, lit("Resource should be inherited from QnVirtualCameraResource"));
    if (!virtualResource)
        return false;

    auto auth = virtualResource->getAuth();
    nx_http::AuthInfo authInfo;

    authInfo.username = auth.user();
    authInfo.password = auth.password();

    m_asyncHttpClient = nx_http::AsyncHttpClient::create();
    m_asyncHttpClient->setResponseReadTimeoutMs(kHttpReceivetimeout.count());
    m_asyncHttpClient->setSendTimeoutMs(kHttpSendTimeout.count());
    m_asyncHttpClient->setAuth(authInfo);

    return true;
}

void FlirWsIOManager::routeIOMonitoringInitialization(InitState newState)
{
    m_initializationState = newState;
    switch (m_initializationState)
    {
        case InitState::initial:
            tryToGetNexusSettings();
        break;
        case InitState::settingsRequested:
            tryToEnableNexusServer();
        break;
        case InitState::nexusServerEnabled:
            connectControlWebsocket();
        break;
        case InitState::controlSocketConnected:
            requestSessionId();
        break;
        case InitState::sessionIdObtained:
            connectNotificationWebSocket();
        break;
        case InitState::notificationSocketConnected:
            subscribeToNotifications();
        break;
        case InitState::subscribed:
            qDebug() << "Everything is fine, it's time to do something dude";
        break;
        case InitState::error:
            qDebug() << "Some error occurred, fix it";
        break;
        default:
            NX_ASSERT(false, "We should never be here.");
    }
    
}