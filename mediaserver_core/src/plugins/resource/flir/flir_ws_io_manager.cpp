#include <chrono>

#include "flir_ws_io_manager.h"

namespace {
    const QString kConfigurationFile("/api/server/status/full");
    const QString kControlPrefix = lit("Nexus.cgi?action=");
    const QString kSubscriptionPrefix = lit("NexusWS_Status.cgi?");
    const QString kServerWhoAmICommand = lit("SERVERWhoAmI");

    const QString kSessionParamName = lit("session");
    const QString kSubscriptionNumParamName = lit("numSubscriptions");
    const QString kNotificationFormatParamName = lit("NotificationFormat");

    const QString kJsonNotificationFormat = lit("JSON");
    const QString kStringNotificationFormat = lit("String");

    const std::chrono::milliseconds kKeepAliveTimeout(10000);
    const std::chrono::milliseconds kMinNotificationInterval(500);
    const std::chrono::milliseconds kMaxNotificationInterval(500);

    const int kAnyDevice = -1;
    const QString kAlarmSubscription = lit("ALARM");
    const QString kIOSubscription = lit("IO");
    const QString kThgSpotSubscription = lit("THGSPOT");
    const QString kThgAreaSubscription = lit("THGAREA");

    const int kMdDeviceType = 12;
    const int kIODeviceType = 28;
    const int kThgSpotDeviceType = 53;
    const int kThgAreaDeviceType = 54;

} //< anonymous namespace

using namespace nx::utils;

FlirWsIOManager::FlirWsIOManager():
    m_nexusSessionId(-1),
    m_monitoringIsInProgress(false),
    m_controlWebSocket(new QWebSocket()),
    m_notificationWebSocket(new QWebSocket())
{

}


FlirWsIOManager::~FlirWsIOManager()
{

}


bool FlirWsIOManager::startIOMonitoring()
{
    //TODO: #dmsihin need mutex here
    if (m_monitoringIsInProgress)
        return true;

    m_monitoringIsInProgress = true;

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
    return true;
}


bool FlirWsIOManager::isMonitoringInProgress() const
{
    return true;
}


QnIOPortDataList FlirWsIOManager::getOutputPortList() const
{
    return QnIOPortDataList();
}


QnIOPortDataList FlirWsIOManager::getInputPortList() const
{
    return QnIOPortDataList();
}


QnIOStateDataList FlirWsIOManager::getPortStates() const
{
    return QnIOStateDataList();
}


nx_io_managment::IOPortState FlirWsIOManager::getPortDefaultState(const QString& portId) const
{
    return nx_io_managment::IOPortState();
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

void FlirWsIOManager::at_connected()
{
    qDebug() << "Websocket connected";

    requestControlToken();
    Qn::directConnect(
        m_notificationWebSocket.get(), &QWebSocket::textMessageReceived,
        this, &FlirWsIOManager::parseNotification);

}

void FlirWsIOManager::at_disconnected()
{
    qDebug() << "Websocket disconnected";
}

bool FlirWsIOManager::initiateWsConnection()
{
    Qn::directConnect(
        m_controlWebSocket.get(), &QWebSocket::connected,
        this, &FlirWsIOManager::at_connected);

    Qn::directConnect(
        m_controlWebSocket.get(), &QWebSocket::disconnected,
        this, &FlirWsIOManager::at_disconnected);

    //Connect here

    return true;
}

void FlirWsIOManager::requestControlToken()
{
    if (!m_monitoringIsInProgress)
        return;

    auto message = kControlPrefix + kServerWhoAmICommand;

    auto paresResponseAndScheduleKeepAlive = [this](const QString& message)
        {
            auto sendKeepAliveWrapper =
                [this](nx::utils::TimerId timerId)
                {
                    if (timerId != m_timerId)
                        return;

                    // It's QObject::disconnect, does not related to some "network disconnect"
                    m_controlWebSocket->disconnect();
                    sendKeepAlive();
                };

            m_nexusSessionId = parseControlMessage(message);
            TimerManager::instance()->addTimer(
                sendKeepAliveWrapper,
                kKeepAliveTimeout);
        };

    Qn::directConnect(
        m_controlWebSocket.get(), &QWebSocket::textMessageReceived,
        this, paresResponseAndScheduleKeepAlive); //< Am not sure this is right. Looks like it's required to use member functions

    auto bytesSent = m_controlWebSocket->sendTextMessage(message);
    //TODO: #dmishin check number of sent bytes;
}

qint64 FlirWsIOManager::parseControlMessage(const QString& message)
{
    return 1;
}

QString FlirWsIOManager::buildNotificationSubsctionString(const QString& subscriptionType)
{
    const int kPeriodicNotificationsMode = 0;

    auto subscriptionString = lit("%1,%2,%3,%4,%5")
        .arg(subscriptionType)
        .arg(kAnyDevice)
        .arg(kMinNotificationInterval.count())
        .arg(kMaxNotificationInterval.count())
        .arg(kPeriodicNotificationsMode);

    return subscriptionString;
}

void FlirWsIOManager::subscribeToNotifications()
{
    QUrlQuery query;

    const int kSubscriptionNumber = 1;

    query.addQueryItem(kSessionParamName, QString::number(m_nexusSessionId));
    query.addQueryItem(kSubscriptionNumParamName, QString::number(kSubscriptionNumber));
    query.addQueryItem(kNotificationFormatParamName, kStringNotificationFormat);
    query.addQueryItem(lit("subscription1"), buildNotificationSubsctionString(kAlarmSubscription));

    auto subscriptionMessage = kSubscriptionPrefix + query.toString();

    auto bytesSent = m_notificationWebSocket->sendTextMessage(subscriptionMessage);
    //TODO: #dmishin check number of sent bytes
}

FlirWsIOManager::FlirAlarmNotification FlirWsIOManager::parseNotification(const QString& message)
{
    FlirAlarmNotification notification;
    auto parts = message.split(L',');

    const std::size_t kPartsCount = 14;
    NX_ASSERT(
        parts.size() == kPartsCount,
        lm("Number of fields is %1, expected %2")
            .arg(parts.size())
            .arg(kPartsCount));

    notification.deviceId = parts[1].toInt();
    notification.health = parts[2].toInt();
    notification.lastBIT = parts[3].toInt();
    notification.timestamp = parseFlirDateTime(parts[4]);
    notification.deviceType = parts[5].toInt();
    notification.sourceIndex = parts[6].toInt();


    return notification;
}

quint64 FlirWsIOManager::parseFlirDateTime(const QString& dateTime)
{
    return 0;
}

void FlirWsIOManager::sendKeepAlive()
{
    if (!m_monitoringIsInProgress)
        return;

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
