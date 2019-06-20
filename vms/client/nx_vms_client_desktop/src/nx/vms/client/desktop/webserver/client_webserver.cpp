#include "client_webserver.h"

#include <QtCore/QUrlQuery>
#include <QtCore/QJsonObject>

#include <QtHttpServer.h>
#include <QtHttpRequest.h>
#include <QtHttpReply.h>

#include <nx/vms/client/desktop/director/director_json_interface.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/app_info.h>

namespace {
const QString kServerName = nx::utils::AppInfo::productNameLong() + " Desktop Client HTTP Server";
const auto kServerBinding = QHostAddress::LocalHost;

QJsonDocument convertHttpToJsonRequest(const QtHttpRequest& request)
{
    QString parameters;
    if (request.getUrl().hasQuery())
        parameters = request.getUrl().query(QUrl::FullyEncoded); //< GET parameters have priority.

    if (parameters.isEmpty())
        parameters = request.getRawData(); //< Use POST if no GET.

    const auto items = QUrlQuery(parameters).queryItems();
    QJsonObject result;
    for (const auto& item: items)
        result.insert(item.first, item.second);

    return QJsonDocument(result);
}
} // namespace

namespace nx::vmx::client::desktop {

DirectorWebserver::DirectorWebserver(QObject* parent):
    QObject(parent),
    m_server(new QtHttpServer(this)),
    m_directorInterface(new DirectorJsonInterface(this))
{
    m_server->setServerName(kServerName);
    connect(m_server.data(), &QtHttpServer::started,
        this, [this] { m_active = true; });
    connect(m_server.data(), &QtHttpServer::stopped,
        this, [this] { m_active = false; });
    connect(m_server.data(), &QtHttpServer::requestNeedsReply,
        this, &DirectorWebserver::processRequest);
}

DirectorWebserver::~DirectorWebserver()
{
    stop();
}

bool DirectorWebserver::setListenAddress(const QString& address, int port)
{
    if (address.isEmpty())
        m_address = QHostAddress(kServerBinding);
    else
        m_address = address;
    m_port = port;

    if (m_active)
    {
        NX_VERBOSE(this, "setListenAddress(%1) - restarting running server due to address change");
        stop();
        start();
    }

    if (m_address.isNull())
        return false;
    return true;
}

bool DirectorWebserver::start()
{
    if (m_address.isNull())
    {
        NX_ERROR(this, "start() - got empty address. I will not start");
        return false;
    }

    m_server->start(m_port, m_address);
    if (!m_active)
        NX_ERROR(this, "start() - failed to start director at a \"%1:%2\"", m_address, m_port);
    else
        NX_INFO(this, "start() - listening to \"%1:%2\"", m_address, m_port);
    return m_active;
}

void DirectorWebserver::stop()
{
    m_server->stop();
}

void DirectorWebserver::onClientConnected(const QString& guid)
{
    NX_VERBOSE(this, "onClientConnected(%1)", guid);
}

void DirectorWebserver::onClientDisconnected(const QString& guid)
{
    NX_VERBOSE(this, "onClientDisconnected(%1)", guid);
}

void DirectorWebserver::onServerError(const QString& errorMessage)
{
    NX_ERROR(this, "onServerError(): %1", errorMessage);
}

void DirectorWebserver::processRequest(QtHttpRequest* request, QtHttpReply* reply)
{
    reply->setStatusCode(QtHttpReply::Ok); //< This line is actually extraneous.
    reply->addHeader("Content-Type", QByteArrayLiteral ("application/json"));

    const auto jsonRequest = convertHttpToJsonRequest(*request);
    auto jsonReply = m_directorInterface->process(jsonRequest);
    reply->appendRawData(jsonReply.toJson(QJsonDocument::Compact));
}

} // namespace nx::vmx::client::desktop
