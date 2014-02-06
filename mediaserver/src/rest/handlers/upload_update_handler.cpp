#include "upload_update_handler.h"

int QnRestUploadUpdateHandler::executeGet(const QString &path, const QnRequestParamList &params, QByteArray &result, QByteArray &contentType) {

}

int QnRestUploadUpdateHandler::executePost(const QString &path, const QnRequestParamList &params, const QByteArray &body, QByteArray &result, QByteArray &contentType) {
    qDebug() << path << params << contentType;
}

QString QnRestUploadUpdateHandler::description() const {

}
