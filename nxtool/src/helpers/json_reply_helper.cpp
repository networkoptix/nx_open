#include "json_reply_helper.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

int rtu::helpers::getJsonReplyError(const QByteArray &jsonData, QString *errorString)
{
    QJsonParseError error;
    QJsonDocument json = QJsonDocument::fromJson(jsonData, &error);

    // Return 0 for invalid JSON replies (it may be not JSON at all).
    if (error.error != QJsonParseError::NoError)
        return 0;

    if (!json.isObject())
        return 0;

    QJsonObject object = json.object();
    QJsonValue errorCodeValue = object.value("error");
    int errorCode = errorCodeValue.isString() ? errorCodeValue.toString().toInt() : errorCodeValue.toInt();

    if (errorCode && errorString)
        *errorString = object.value("errorString").toString();

    return errorCode;
}
