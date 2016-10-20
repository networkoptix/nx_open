#include <chrono>

#include "flir_ws_io_manager.h"

namespace {
    const QString kCommandPrefix = lit("Nexus.cgi?action=");
    const QString kServerWhoAmICommand = lit("SERVERWhoAmI");
    const QString kSessionParamName = lit("session");
    const std::chrono::milliseconds kKeepAliveTimeout(10000);
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

}


void FlirWsIOManager::setInputPortStateChangeCallback(QnAbstractIOManager::InputStateChangeCallback callback)
{

}


void FlirWsIOManager::setNetworkIssueCallback(QnAbstractIOManager::NetworkIssueCallback callback)
{

}


void FlirWsIOManager::terminate()
{

}

void FlirWsIOManager::at_connected()
{
    qDebug() << "Websocket connected";

    requestControlToken();
    QObject::connect(
        m_notificationWebSocket.get(), &QWebSocket::textMessageReceived,
        this, &FlirWsIOManager::parseNotification);

}

void FlirWsIOManager::at_disconnected()
{
    qDebug() << "Websocket disconnected";
}

bool FlirWsIOManager::initiateWsConnection()
{
    QObject::connect(
        m_controlWebSocket.get(), &QWebSocket::connected,
        this, &FlirWsIOManager::at_connected);

    QObject::connect(
        m_controlWebSocket.get(), &QWebSocket::disconnected,
        this, &FlirWsIOManager::at_disconnected);

    //Connect here

    return true;
}

void FlirWsIOManager::requestControlToken()
{
    if (!m_monitoringIsInProgress)
        return;

    auto message = kCommandPrefix + kServerWhoAmICommand;

    auto paresResponseAndScheduleKeepAlive = [this](const QString& message)
        {
            //TODO: #dmishin parse response
            m_nexusSessionId = parseControlMessage(message);
            TimerManager::instance()->addTimer(
                [this](nx::utils::TimerId timerId)
                {
                    if (timerId != m_timerId)
                        return;

                    // It's QObject::disconnect, does not related to some "network disconnect"
                    m_controlWebSocket->disconnect();
                    sendKeepAlive();
                },
                kKeepAliveTimeout);
        };

    QObject::connect(
        m_controlWebSocket.get(), &QWebSocket::textMessageReceived,
        paresResponseAndScheduleKeepAlive);

    auto bytesSent = m_controlWebSocket->sendTextMessage(message);
    //TODO: #dmishin check number of sent bytes;
}

qint64 FlirWsIOManager::parseControlMessage(const QString& message)
{
    return 1;
}

void FlirWsIOManager::parseNotification(const QString& message)
{

}

void FlirWsIOManager::sendKeepAlive()
{
    if (!m_monitoringIsInProgress)
        return;

    auto keepAliveMessage = lit("%1%2&%3=%4")
        .arg(kCommandPrefix)
        .arg(kServerWhoAmICommand)
        .arg(kSessionParamName)
        .arg(m_nexusSessionId);

    auto bytesSent = m_controlWebSocket->sendTextMessage(keepAliveMessage);
    //TODO: #dmishin check number of sent bytes;

    m_timerId = TimerManager::instance()->addTimer(
        [this](nx::utils::TimerId timerId)
        {
            if (timerId != m_timerId)
                return;

            sendKeepAlive();
        },
        kKeepAliveTimeout);
}
