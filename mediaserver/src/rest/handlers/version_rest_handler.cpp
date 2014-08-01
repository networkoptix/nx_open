#include "version_rest_handler.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"
#include "version.h"

int QnVersionRestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(path)
    Q_UNUSED(params)
    Q_UNUSED(contentType)

    result.append(QString("<root><engineVersion>%1</engineVersion><revision>%2</revision></root>\n").arg(QN_ENGINE_VERSION).arg(QN_APPLICATION_REVISION).toUtf8());
    return CODE_OK;
}

int QnVersionRestHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray&, const QByteArray& /*srcBodyContentType*/, QByteArray& result, QByteArray& contentType)
{
    return executeGet(path, params, result, contentType);
}
