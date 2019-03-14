#include "favicon_rest_handler.h"

#include <QtCore/QFile>

#include "network/tcp_connection_priv.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"

#include <media_server/serverutil.h>

static const int READ_BLOCK_SIZE = 1024*512;

int QnFavIconRestHandler::executeGet(const QString& /*path*/, const QnRequestParamList& /*params*/,
    QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*)
{
    QFile f(":/static/customization/favicon.ico");
    if (f.open(QFile::ReadOnly))
    {
        result = f.readAll();
        contentType = "image/vnd.microsoft.icon";
        return nx::network::http::StatusCode::ok;
    }
    else {
        return nx::network::http::StatusCode::notFound;
    }
}

int QnFavIconRestHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& /*body*/, const QByteArray& /*srcBodyContentType*/, QByteArray& result,
                                      QByteArray& contentType, const QnRestConnectionProcessor* owner)
{
    return executeGet(path, params, result, contentType, owner);
}
