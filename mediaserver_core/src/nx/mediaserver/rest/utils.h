#pragma once

#include <rest/server/json_rest_handler.h>

namespace nx::mediaserver::rest {

nx::network::http::StatusCode::Value errorToHttpStatusCode(QnRestResult::Error error);

JsonRestResponse makeResponse(QnRestResult::Error error, const QString& errorString);

JsonRestResponse makeResponse(QnRestResult::Error error, const QStringList& arguments);

QMap<QString, QString> variantMapToStringMap(const QVariantMap& variantMap);

} // namespace nx::mediaserver::rest
