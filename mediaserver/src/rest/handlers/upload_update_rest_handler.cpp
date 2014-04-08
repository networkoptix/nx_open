#include "upload_update_rest_handler.h"

int QnUploadUpdateRestHandler::executeGet(const QString &path, const QnRequestParamList &params, QByteArray &result, QByteArray &contentType) {

}

int QnUploadUpdateRestHandler::executePost(const QString &path, const QnRequestParamList &params, const QByteArray &body, QByteArray &result, QByteArray &contentType) {
    qDebug() << path << params << contentType;
}

QString QnUploadUpdateRestHandler::description() const {

}
