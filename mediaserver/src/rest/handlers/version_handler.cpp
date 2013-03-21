#include "version_handler.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"
#include "version.h"

int QnVersionHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(path)
    Q_UNUSED(params)
    Q_UNUSED(contentType)

    result.append(QString("<root><engineVersion>%1</engineVersion><revision>%2</revision></root>\n").arg(QN_ENGINE_VERSION).arg(QN_APPLICATION_REVISION).toUtf8());
    return CODE_OK;
}

int QnVersionHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray&, QByteArray& result, QByteArray& contentType)
{
    return executeGet(path, params, result, contentType);
}

QString QnVersionHandler::description(TCPSocket *) const
{
    return "Returns server version";
}
