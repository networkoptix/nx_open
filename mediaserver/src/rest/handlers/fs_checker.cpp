#include <QFileInfo>
#include "utils/network/tcp_connection_priv.h"
#include "rest/server/rest_server.h"
#include "core/resourcemanagment/resource_pool.h"
#include "utils/common/util.h"
#include "api/serializer/serializer.h"
#include "fs_checker.h"

int QnFsHelperHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result)
{
    QString pathStr;
    QString errStr;

    for (int i = 0; i < params.size(); ++i)
    {
        if (params[i].first == "path")
        {
            pathStr = params[i].second;
            break;
        }
    }

    if (pathStr.isEmpty())
        errStr += "Parameter path is absent or empty. \n";

    if (!errStr.isEmpty())
    {
        result.append("<root>\n");
        result.append(errStr);
        result.append("</root>\n");
        return CODE_INVALID_PARAMETER;
    }

    QnStorageResource* storage = QnStoragePluginFactory::instance()->createStorage(pathStr, false);
    int prefixPos = pathStr.indexOf("://");
    QString prefix;
    if (prefixPos != -1)
        prefix = pathStr.left(prefixPos+3);
    if (storage == 0)
    {
        result.append("<root>\n");
        result.append(QString("Unknown storage plugin '%1'").arg(prefix));
        result.append("</root>\n");
        return CODE_INVALID_PARAMETER;
    }

    pathStr = pathStr.mid(prefix.length());
    storage->setUrl(pathStr);
    QString rezStr = storage->isStorageAvailableForWriting() ? "OK" : "FAIL";

    result.append("<root>\n");
    result.append(rezStr);
    result.append("</root>\n");

    delete storage;

    return CODE_OK;
}

int QnFsHelperHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result)
{
    return executeGet(path, params, result);
}

QString QnFsHelperHandler::description(TCPSocket* tcpSocket) const
{
    QString rez;
    rez += "Returns 'OK' if specified folder may be used for writing on mediaServer. Otherwise returns 'FAIL' \n";
    rez += "<BR>Param <b>path</b> - Folder.";
    rez += "<BR><b>Return</b> XML</b> - with 'OK' or 'FAIL' message";
    return rez;
}
