#include "join_system_tool.h"

#include <QtCore/QTimer>

#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "api/app_server_connection.h"
#include "nx_ec/dummy_handler.h"
#include "utils/network/module_finder.h"

namespace {

    const int checkTimeout = 60 * 1000;

    ec2::AbstractECConnectionPtr connection2() {
        return QnAppServerConnectionFactory::getConnection2();
    }

} // anonymous namespace

QnJoinSystemTool::QnJoinSystemTool(QObject *parent) :
    QObject(parent),
    m_timer(new QTimer(this))
{
    m_timer->setInterval(checkTimeout);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &QnJoinSystemTool::at_timer_timeout);
}

bool QnJoinSystemTool::isRunning() const {
    return m_running;
}

void QnJoinSystemTool::start(const QUrl &url, const QString &password) {
    m_targetUrl = url;
    m_password = password;

    m_running = true;
    connect(qnResPool, &QnResourcePool::resourceAdded, this, &QnJoinSystemTool::at_resource_added);

    connection2()->getDiscoveryManager()->discoverPeer(m_targetUrl, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
    m_timer->start();
}

void QnJoinSystemTool::finish(int errorCode) {
    m_running = false;
    emit finished(errorCode);
}

void QnJoinSystemTool::at_resource_added(const QnResourcePtr &resource) {
    if (!m_running)
        return;

    if (!resource->hasFlags(QnResource::server))
        return;

    QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();

    if (QUrl(server->getApiUrl()).port(-1) != m_targetUrl.port())
        return;

    if (!server->getNetAddrList().contains(m_address))
        return;


}

void QnJoinSystemTool::at_timer_timeout() {
    finish(Timeout);
}
