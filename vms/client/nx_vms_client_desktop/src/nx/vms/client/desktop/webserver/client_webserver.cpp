#include "client_webserver.h"

#include <QtCore/QUrlQuery>
#include <QtCore/QJsonObject>

#include <QtHttpServer.h>
#include <QtHttpRequest.h>
#include <QtHttpReply.h>


#include <nx/vms/client/desktop/director/director_json_interface.h>
#include <qdnslookup.h>

namespace {
const QString kServerName = "NX Client HTTP Server";
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

ClientWebserver::ClientWebserver(QObject* parent):
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
        this, &ClientWebserver::makeReply);
}

ClientWebserver::~ClientWebserver()
{
    stop();
}

bool ClientWebserver::start(int port)
{
    if (port != 0)
        m_port = port;

    m_server->start(m_port, kServerBinding);
    return m_active;
}

void ClientWebserver::stop()
{
    m_server->stop();
}

void ClientWebserver::makeReply(QtHttpRequest* request, QtHttpReply* reply)
{
    reply->setStatusCode(QtHttpReply::Ok); //< This line is actually extraneous.
    reply->addHeader("Content-Type", QByteArrayLiteral ("application/json"));

    const auto jsonRequest = convertHttpToJsonRequest(*request);
    auto jsonReply = m_directorInterface->process(jsonRequest);
    reply->appendRawData(jsonReply.toJson(QJsonDocument::Compact));
}

} // namespace nx::vmx::client::desktop
