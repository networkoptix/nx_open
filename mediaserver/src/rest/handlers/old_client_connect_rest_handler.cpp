#include "old_client_connect_rest_handler.h"
#include <utils/network/http/httptypes.h>
#include <utils/common/app_info.h>
#include <QFile>

int QnOldClientConnectRestHandler::executeGet(const QString &path, const QnRequestParamList &params, QByteArray &responseMessageBody, QByteArray &contentType)
{
    Q_UNUSED(responseMessageBody)
    Q_UNUSED(contentType)
    QFile f(":/pb_connect.bin");
    if (f.open(QFile::ReadOnly)) 
    {
        contentType="application/x-protobuf";

        QByteArray pattern =  f.readAll();
        pattern = pattern.remove(0, 9); // remove version from predefined pattern

        char ch = 0xa;
        responseMessageBody.append(&ch, 1); // string field type
        ch = QnAppInfo::applicationVersion().length();
        responseMessageBody.append(&ch, 1); // string field len
        responseMessageBody.append(QnAppInfo::applicationVersion());
        responseMessageBody.append(pattern);

        return nx_http::StatusCode::ok;
    }
    else {
        return nx_http::StatusCode::notFound;
    }

}

int QnOldClientConnectRestHandler::executePost(const QString &, const QnRequestParamList &, const QByteArray &, const QByteArray& srcBodyContentType, QByteArray &, QByteArray &)
{
    return nx_http::StatusCode::notImplemented;
}
