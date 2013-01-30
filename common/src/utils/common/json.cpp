#include "json.h"

#include <algorithm> /* For std::copy & std::transform. */
#include <functional> /* For std::mem_fun_ref. */

#include <qjson/serializer.h>
#include <qjson/parser.h>

void QJson_detail::serialize_json(const QVariant &value, QByteArray *target) {
    QJson::Serializer serializer;
    *target = serializer.serialize(value);
}

bool QJson_detail::deserialize_json(const QByteArray &value, QVariant *target) {
    QJson::Parser deserializer;

    bool ok = true;
    QVariant result = deserializer.parse(value, &ok);
    if(!ok)
        return false;

    *target = result;
    return true;
}

