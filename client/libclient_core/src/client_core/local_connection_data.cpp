#include "local_connection_data.h"

#include <nx/fusion/model_functions.h>

#include <client_core/client_core_settings.h>
#include <nx/network/socket_global.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (QnLocalConnectionData)(QnWeightData), (datastream)(eq)(json), _Fields)

QnLocalConnectionData::QnLocalConnectionData() {}

QnLocalConnectionData::QnLocalConnectionData(
    const QString& systemName,
    const QnUuid& localId,
    const QUrl& url)
    :
    systemName(systemName),
    localId(localId),
    url(url),
    password(url.password())
{
    this->url.setPassword(QString());
}

bool QnLocalConnectionData::isStoredPassword() const
{
    return !password.isEmpty();
}

QUrl QnLocalConnectionData::urlWithPassword() const
{
    auto url = this->url;
    url.setPassword(password.value());
    return url;
}

bool helpers::storeLocalSystemConnection(
    const QString& systemName,
    const QnUuid& localSystemId,
    const QUrl& url,
    QnLocalConnectionData& result)
{
    const bool connectionToCloud = nx::network::SocketGlobals::addressResolver().isCloudHostName(url.host());
    NX_ASSERT(!connectionToCloud, "Can't store connection to cloud as recent local");
    if (connectionToCloud)
        return false;

    // TODO: #ynikitenkov remove outdated connection data

    auto recentConnections = qnClientCoreSettings->recentLocalConnections();
    const auto itEnd = std::remove_if(recentConnections.begin(), recentConnections.end(),
        [localSystemId, userName = url.userName()](const QnLocalConnectionData& connection)
        {
            return (connection.localId == localSystemId)
                && (QString::compare(connection.url.userName(), userName, Qt::CaseInsensitive) == 0);
        });

    recentConnections.erase(itEnd, recentConnections.end());

    result = QnLocalConnectionData(systemName, localSystemId, url);
    recentConnections.prepend(result);

    qnClientCoreSettings->setRecentLocalConnections(recentConnections);
    return true;
}

void helpers::forgetLocalConnectionPassword(const QnUuid& localId, const QString& userName)
{
    auto recentConnections = qnClientCoreSettings->recentLocalConnections();
    const auto it = std::find_if(recentConnections.begin(), recentConnections.end(),
        [localId, userName](const QnLocalConnectionData& connection)
        {
            return ((connection.localId == localId)
                && (QString::compare(connection.url.userName(), userName, Qt::CaseInsensitive) == 0));
        });

    if (it == recentConnections.end())
        return;

    if (it->url.password().isEmpty() && it->password.isEmpty())
        return;

    auto& data = *it;
    data.url.setPassword(QString());
    data.password.setValue(QString());
    qnClientCoreSettings->setRecentLocalConnections(recentConnections);
    qnClientCoreSettings->save();
}

