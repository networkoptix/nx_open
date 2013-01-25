#include "json.h"

#include <algorithm> /* For std::copy & std::transform. */
#include <functional> /* For std::mem_fun_ref. */

#include <qjson/serializer.h>
#include <qjson/parser.h>

void serialize(const QStringList &value, QVariant *target) {
    assert(target);
    
    QVariantList list;
    std::copy(value.begin(), value.end(), std::back_inserter(list));
    *target = list;
}

bool deserialize(const QVariant &value, QStringList *target) {
    assert(target);

    if(value.type() != QVariant::List)
        return false;

    QVariantList list = value.toList();
    QStringList result;
    std::transform(list.begin(), list.end(), std::back_inserter(result), std::mem_fun_ref(&QVariant::toString));
    *target = result;
    return true;
}

void serialize(const QVariantMap &value, QVariant *target) {
    assert(target);

    *target = value;
}

bool deserialize(const QVariant &value, QVariantMap *target) {
    assert(target);

    if(value.type() != QVariant::Map) 
        return false;
    
    *target = value.toMap();
    return true;
}


void QJson::detail::json_serialize(const QVariant &value, QByteArray *target) {
    assert(target);

    QJson::Serializer serializer;
    *target = serializer.serialize(value);
}

bool QJson::detail::json_deserialize(const QByteArray &value, QVariant *target) {
    assert(target);

    QJson::Parser deserializer;

    bool ok = true;
    QVariant result = deserializer.parse(value, &ok);
    if(!ok)
        return false;

    *target = result;
    return true;
}

