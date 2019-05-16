#include "client_webserver.h"

#include <QtCore/QUrlQuery>
#include <QtCore/QJsonObject>

#include <QtHttpServer.h>
#include <QtHttpRequest.h>
#include <QtHttpReply.h>

#include <nx/vms/client/desktop/director/director_json_interface.h>
#include <nx/utils/log/assert.h>
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
        this, &DirectorWebserver::makeReply);
}

DirectorWebserver::~DirectorWebserver()
{
    stop();
}

void DirectorWebserver::setPort(int port)
{
    if (!NX_ASSERT(port > 0 && port < 65536, "Client webserver port number should be 1..65535"))
        return;

    m_port = port;

    if (m_active)
    {
        stop();
        start();
    }
}

bool DirectorWebserver::start()
{
    m_server->start(m_port, kServerBinding);
    return m_active;
}

void DirectorWebserver::stop()
{
    m_server->stop();
}

void DirectorWebserver::makeReply(QtHttpRequest* request, QtHttpReply* reply)
{
    reply->setStatusCode(QtHttpReply::Ok); //< This line is actually extraneous.
    reply->addHeader("Content-Type", QByteArrayLiteral ("application/json"));

    const auto jsonRequest = convertHttpToJsonRequest(*request);
    auto jsonReply = m_directorInterface->process(jsonRequest);
    reply->appendRawData(jsonReply.toJson(QJsonDocument::Compact));
}

} // namespace nx::vmx::client::desktop
