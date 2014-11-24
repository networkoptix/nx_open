#include "old_client_connect_rest_handler.h"
#include <utils/network/http/httptypes.h>
#include <utils/common/app_info.h>
#include <QFile>

namespace {
    const int versionOffset = 0x1;
    const int productOffset = 0x47d;
}

int QnOldClientConnectRestHandler::executeGet(const QString &path, const QnRequestParamList &params, QByteArray &responseMessageBody, QByteArray &contentType, const QnRestConnectionProcessor*) 
{
    Q_UNUSED(path)
    Q_UNUSED(params)

    QFile f(":/pb_connect.bin");
    if (!f.open(QFile::ReadOnly))
        return nx_http::StatusCode::notFound;

    contentType = "application/x-protobuf";

    responseMessageBody = f.readAll();
    char oldLength, newLength;

    /* replace product name */
    QByteArray product = QnAppInfo::productNameShort().toLatin1();
    newLength = product.length();
    oldLength = responseMessageBody[productOffset];
    responseMessageBody[productOffset] = newLength;
    responseMessageBody.replace(productOffset + 1, oldLength, product);

    /* replace version */
    QByteArray version = QnAppInfo::applicationVersion().toLatin1();
    newLength = version.length();
    oldLength = responseMessageBody[versionOffset];
    responseMessageBody[versionOffset] = newLength;
    responseMessageBody.replace(versionOffset + 1, oldLength, version);

    return nx_http::StatusCode::ok;
}

int QnOldClientConnectRestHandler::executePost(const QString &, const QnRequestParamList &, const QByteArray &, const QByteArray &, QByteArray &, QByteArray &, const QnRestConnectionProcessor*) 
{
    return nx_http::StatusCode::notImplemented;
}
