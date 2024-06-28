// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "js_utils.h"

#include <QtCore/QRect>

namespace nx::vms::client::core::testkit {

namespace {

template <typename R>
QJsonValue rectToJson(const R& r)
{
    return QJsonObject{
        {"x", r.x()},
        {"y", r.y()},
        {"width", r.width()},
        {"height", r.height()}
    };
}

} // namespace

QJsonObject makeErrorResponse(const QString& message)
{
    return QJsonObject{
        {"error", 1},
        {"errorString", message}
    };
}

QJsonObject makeResponse(const QJSValue& result)
{
    // Return error description if the script fails.
    if (result.isError())
        return makeErrorResponse(result.toString());

    // Construct JSON representation of the result.
    QJsonObject response;
    const auto resultVariant = result.toVariant();
    auto jsonResult = resultVariant.toJsonValue();

    // Complex values like QObject don't have JSON representation,
    // so we need to provide additional type information.
    // For convinience type information is provided for all values.

    // Make sure "result" field always exists.
    if (jsonResult.isUndefined())
        response.insert("result", result.toString());
    else
        response.insert("result", jsonResult);

    const char* typeName = "";

    if (result.isUndefined())
    {
        typeName = "undefined";
    }
    else if (result.isCallable())
    {
        typeName = "function";
    }
    else if (result.isString())
    {
        typeName = "string";
    }
    else if (result.isBool())
    {
        typeName = "boolean";
    }
    else if (result.isNumber())
    {
        typeName = "number";
    }
    else if (result.isQObject())
    {
        typeName = "qobject";
        if (const auto o = result.toQObject())
            response.insert("metatype", o->metaObject()->className());
    }
    else if (result.isArray())
    {
        typeName = "array";
        response.insert("length", result.property("length").toInt());
    }
    else if (result.isObject())
    {
        typeName = "object";
        if (const auto t = resultVariant.typeName())
        {
            // QRect* does not expose its fields to the script,
            // so we just construct the JS objects with necessary fields.
            response.insert("metatype", t);
            if (resultVariant.typeId() == qMetaTypeId<QRect>())
                response.insert("result", rectToJson(resultVariant.toRect()));
            else if (resultVariant.typeId() == qMetaTypeId<QRectF>())
                response.insert("result", rectToJson(resultVariant.toRectF()));
        }
    }
    else if (result.isNull())
    {
        typeName = "null";
    }
    else if (result.isVariant())
    {
        typeName = "variant";
    }
    else if (result.isRegExp())
    {
        typeName = "regexp";
    }
    else if (result.isQMetaObject())
    {
        typeName = "qmetaobject";
    }
    else if (result.isDate())
    {
        typeName = "date";
    }
    else if (result.isError())
    {
        typeName = "error";
    }

    response.insert("type", typeName);

    return response;
}

} // namespace nx::vms::client::core::testkit
