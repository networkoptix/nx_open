#include "json.h"

#include <algorithm> /* For std::copy & std::transform. */
#include <functional> /* For std::mem_fun_ref. */

#include <QtCore/QMetaType>
#include <QtGui/QColor>
#include <QUuid>

#include <qjson/serializer.h>
#include <qjson/parser.h>

#include <utils/common/color.h>

#include "warnings.h"

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

void serialize(const QUuid &value, QVariant *target) {
    *target = value.toString();
}

bool deserialize(const QVariant &value, QUuid *target) {
    /* Support JSON null for QUuid, even though we don't generate it on
     * serialization. */
    if(value.type() == QVariant::Invalid) {
        *target = QUuid();
        return true;
    }

    if(value.type() != QVariant::String)
        return false;

    QString string = value.toString();
    QUuid result(string);
    if(result.isNull() && string != QLatin1String("00000000-0000-0000-0000-000000000000") && string != QLatin1String("{00000000-0000-0000-0000-000000000000}"))
        return false;

    *target = result;
    return true;
}

void serialize(const QColor &value, QVariant *target) {
    *target = value.name();
}

bool deserialize(const QVariant &value, QColor *target) {
    if(value.type() != QVariant::String)
        return false;
    *target = parseColor(value);
    return true;
}

void serialize(const QVariant &value, QVariant *target) {
    switch(value.userType()) {
    case QVariant::Invalid:         *target = QVariant(); break;
    case QMetaType::QVariantList:   QJson::serialize(value.toList(), target); break;
    case QMetaType::QVariantMap:    QJson::serialize(value.toMap(), target); break;
    case QMetaType::QStringList:    QJson::serialize(value.toStringList(), target); break;
    case QMetaType::QChar:          QJson::serialize(value.toString(), target); break;
    case QMetaType::QString:        QJson::serialize(value.toString(), target); break;
    case QMetaType::Double:      
    case QMetaType::Float:          QJson::serialize(value.toDouble(), target); break;
    case QMetaType::Bool:           QJson::serialize(value.toBool(), target); break;
    case QMetaType::UChar:
    case QMetaType::UShort:
    case QMetaType::UInt:
    case QMetaType::ULong:
    case QMetaType::ULongLong:      QJson::serialize(value.toULongLong(), target); break;
    case QMetaType::Char:
    case QMetaType::Short:
    case QMetaType::Int:
    case QMetaType::Long:
    case QMetaType::LongLong:       QJson::serialize(value.toLongLong(), target); break;
    case QMetaType::QColor:         QJson::serialize(value.value<QColor>(), target); break;
    default:
        qnWarning("Type '%1' is not supported for JSON serialization.", QMetaType::typeName(value.userType()));
        *target = QVariant();
        break;
    }
}

bool deserialize(const QVariant &value, QVariant *target) {
    *target = value;
    return true;
}

void serialize(const QVariantList &value, QVariant *target) {
    QJson_detail::serialize_list<QVariant, QVariantList>(value, target);
}

bool deserialize(const QVariant &value, QVariantList *target) {
    if(value.type() != QVariant::List)
        return false;

    *target = value.toList();
    return true;
}

void serialize(const QVariantMap &value, QVariant *target) {
    QVariantMap result;
    for(QVariantMap::const_iterator pos = value.begin(); pos != value.end(); pos++) {
        QVariant serialized;
        serialize(*pos, &serialized);
        result.insert(pos.key(), serialized);
    }
    *target = result;
}

bool deserialize(const QVariant &value, QVariantMap *target) {
    if(value.type() != QVariant::Map)
        return false;

    *target = value.toMap();
    return true;
}
