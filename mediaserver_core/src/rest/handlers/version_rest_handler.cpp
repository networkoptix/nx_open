#include "version_rest_handler.h"

#include <QtCore/QCoreApplication>

#include "network/tcp_connection_priv.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"
#include "common/common_module.h"
#include <utils/common/app_info.h>
#include <common/static_common_module.h>

int QnAppInfoRestHandler::executeGet(const QString& /*path*/, const QnRequestParamList& /*params*/, QByteArray& result, QByteArray& /*contentType*/, const QnRestConnectionProcessor*)
{
    result.append(QString("<root><engineVersion>%1</engineVersion><revision>%2</revision></root>\n").arg(qnStaticCommon->engineVersion().toString()).arg(QCoreApplication::applicationVersion()).toUtf8());
    return nx::network::http::StatusCode::ok;
}

int QnAppInfoRestHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray&, const QByteArray& /*srcBodyContentType*/, QByteArray& result,
                                      QByteArray& contentType, const QnRestConnectionProcessor* owner)
{
    return executeGet(path, params, result, contentType, owner);
}
