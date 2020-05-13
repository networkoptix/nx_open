#include "json_utils.h"

#include <cmath>
#include <climits>
#include <stdexcept>

#include <nx/utils/log/assert.h>

#include <QtCore/QJsonDocument>

namespace nx::vms_server_plugins::analytics::vivotek {

QJsonValue parseJson(const QByteArray& bytes)
{
    QJsonParseError error;
    auto document = QJsonDocument::fromJson(bytes, &error);
    if (document.isObject())
        return document.object();
    if (document.isArray())
        return document.array();
    throw std::runtime_error(
        QStringLiteral("Failed to parse json: %1")
            .arg(error.errorString())
            .toStdString());
}

QByteArray unparseJson(const QJsonValue& json)
{
    QJsonDocument document;
    if (json.isObject())
        document.setObject(json.toObject());
    else if (json.isArray())
        document.setArray(json.toArray());
    else if (!NX_ASSERT(json.isObject() || json.isArray()))
        document.setObject({});
    return document.toJson(QJsonDocument::Compact);
}


namespace json_utils_detail {

const QString rootPath = "$";


void get(QJsonValue* value, const QString& /*path*/, const QJsonValue& json)
{
    *value = json;
}

void get(bool* value, const QString& path, const QJsonValue& json)
{
    if (!json.isBool())
        throw std::runtime_error(NX_FMT("%1 is not a boolean", path).toStdString());
    *value = json.toBool();
}

void get(double* value, const QString& path, const QJsonValue& json)
{
    if (!json.isDouble())
        throw std::runtime_error(NX_FMT("%1 is not a number", path).toStdString());
    *value = json.toDouble();
}

void get(int* value, const QString& path, const QJsonValue& json)
{
    double doubleValue;
    get(&doubleValue, path, json);
    double roundedDoubleValue = std::round(doubleValue);
    if (roundedDoubleValue != doubleValue)
        throw std::runtime_error(NX_FMT("%1 is not an integer", path).toStdString());
    if (roundedDoubleValue < INT_MIN || INT_MAX < roundedDoubleValue)
        throw std::runtime_error(NX_FMT("%1 is out of int range", path).toStdString());
    *value = roundedDoubleValue;
}

void get(QString* value, const QString& path, const QJsonValue& json)
{
    if (!json.isString())
        throw std::runtime_error(NX_FMT("%1 is not a string", path).toStdString());
    *value = json.toString();
}

void get(QJsonObject* value, const QString& path, const QJsonValue& json)
{
    if (!json.isObject())
        throw std::runtime_error(NX_FMT("%1 is not an object", path).toStdString());
    *value = json.toObject();
}

void get(QJsonArray* value, const QString& path, const QJsonValue& json)
{
    if (!json.isArray())
        throw std::runtime_error(NX_FMT("%1 is not an array", path).toStdString());
    *value = json.toArray();
}

} // namespace json_utils_detail

} // namespace nx::vms_server_plugins::analytics::vivotek
