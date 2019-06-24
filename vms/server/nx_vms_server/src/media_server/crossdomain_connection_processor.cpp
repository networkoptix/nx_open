#include "crossdomain_connection_processor.h"

#include <QtCore/QFile>

#include <api/global_settings.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <network/tcp_connection_priv.h>
#include <network/tcp_listener.h>
#include <utils/common/app_info.h>

#include <nx/network/app_info.h>
#include <nx/network/http/http_types.h>
#include <nx/network/cloud/cloud_connect_controller.h>

namespace {

    static const QByteArray kContentType = "application/xml";
static const QByteArray kIfacePattern = "%SERVER_IF_LIST%";
static const QByteArray kCrossdomainPattern = "%CLOUD_PORTAL_URL%";

} // namespace

class QnCrossdomainConnectionProcessorPrivate : public QnTCPConnectionProcessorPrivate
{
};

QnCrossdomainConnectionProcessor::QnCrossdomainConnectionProcessor(
    std::unique_ptr<nx::network::AbstractStreamSocket> socket,
    QnTcpListener* owner)
    :
    QnTCPConnectionProcessor(
        new QnCrossdomainConnectionProcessorPrivate, std::move(socket), owner)
{
}

QnCrossdomainConnectionProcessor::~QnCrossdomainConnectionProcessor()
{
    stop();
}

void QnCrossdomainConnectionProcessor::run()
{
    Q_D(QnCrossdomainConnectionProcessor);

    initSystemThreadId();

    if (d->clientRequest.isEmpty())
    {
        if (!readRequest())
            return;
    }
    parseRequest();
    d->response.messageBody.clear();

    QnUuid selfId = commonModule()->moduleGUID();
    QnMediaServerResourcePtr mServer = resourcePool()->getResourceById<QnMediaServerResource>(selfId);
    QFile file(":/static/crossdomain.xml");
    if (!mServer || !file.open(QFile::ReadOnly))
    {
        sendResponse(nx::network::http::StatusCode::notFound, kContentType, QByteArray());
        return;
    }

    QList<QByteArray> lines = file.readAll().split('\n');
    for (int i = lines.size() - 1; i >= 0; --i)
    {
        QByteArray pattern = lines[i];
        if (lines[i].contains(kIfacePattern))
        {
            lines.removeAt(i);
            for (const auto& addr: mServer->getAllAvailableAddresses())
            {
                QByteArray allowAccessFromRecord = pattern;
                allowAccessFromRecord.replace(kIfacePattern, addr.address.toString().toUtf8());
                lines.insert(i, allowAccessFromRecord);
            }
        }
        else if (lines[i].contains(kCrossdomainPattern))
        {
            lines.removeAt(i);
            const QString portalUrl =
                QUrl(nx::network::AppInfo::defaultCloudModulesXmlUrl(
                    nx::network::SocketGlobals::cloud().cloudHost())).host();
            if (!portalUrl.isEmpty())
                lines.insert(i, pattern.replace(kCrossdomainPattern, portalUrl.toUtf8()));
        }
    }

    d->response.messageBody = lines.join('\n');
    sendResponse(nx::network::http::StatusCode::ok, kContentType, QByteArray());
}
