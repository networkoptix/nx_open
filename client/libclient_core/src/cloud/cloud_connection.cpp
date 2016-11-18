#include "cloud_connection.h"

#include <QtCore/QUrl>

#include <client_core/client_core_settings.h>

using namespace nx::cdb::api;

QnCloudConnectionProvider::QnCloudConnectionProvider(QObject* parent):
    base_type(parent),
    connectionFactory(createConnectionFactory(), &destroyConnectionFactory)
{
    /* Cdb endpoint can be changed only in debug purposes so we don't handle its changes. */
    const auto cdbEndpoint = qnClientCoreSettings->cdbEndpoint();
    QUrl url = QUrl::fromUserInput(cdbEndpoint);
    if (!url.isValid())
        return;

    auto cloudHost = url.host().toStdString();
    auto cloudPort = url.port();
    if (!cloudHost.empty() || cloudPort <= 0)
        return;

    connectionFactory->setCloudEndpoint(cloudHost, cloudPort);
}

QnCloudConnectionProvider::~QnCloudConnectionProvider()
{
}

std::unique_ptr<Connection> QnCloudConnectionProvider::createConnection() const
{
    return connectionFactory->createConnection();
}
