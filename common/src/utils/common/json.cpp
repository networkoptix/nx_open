#include "json.h"

#include <algorithm> /* For std::copy & std::transform. */
#include <functional> /* For std::mem_fun_ref. */

#include <QtCore/QMetaType>
#include <QtGui/QColor>
#include <QtCore/QUuid>

#include <QtCore/QJsonValue>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>

#include "warnings.h"
#include <utils/common/json_utils.h>

const QLatin1String atomicValue("ATOMIC_VALUE");

void QJson_detail::serialize_json(const QVariant &value, QByteArray *target) {
    switch (value.type()) {
    case QVariant::Map:
    case QVariant::List:
    case QVariant::StringList:
        *target = QJsonDocument::fromVariant(value).toJson();
        return;
    default:
        break;
    }

    QVariantMap map;
    map[atomicValue] = value;
    *target = QJsonDocument::fromVariant(map).toJson();
}

bool QJson_detail::deserialize_json(const QByteArray &value, QVariant *target) {
    QJsonParseError error;
    QVariant result = QJsonDocument::fromJson(value, &error).toVariant();
    if(error.error != QJsonParseError::NoError) {
        qWarning() << error.errorString();
        return false;
    }

    if (result.type() == QVariant::Map && result.toMap().contains(atomicValue))
        *target = result.toMap()[atomicValue];
    else
        *target = result;
    return true;
}

void serialize(const QVariant &value, QVariant *target) {
    switch(value.userType()) {
    case QMetaType::UnknownType:    *target = QVariant(); break;
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
    case QMetaType::QUuid:          QJson::serialize(value.value<QUuid>(), target); break;
    case QMetaType::QSize:          QJson::serialize(value.value<QSize>(), target); break;
    case QMetaType::QSizeF:         QJson::serialize(value.value<QSizeF>(), target); break;
    case QMetaType::QPoint:         QJson::serialize(value.value<QPoint>(), target); break;
    case QMetaType::QPointF:        QJson::serialize(value.value<QPointF>(), target); break;
    case QMetaType::QRect:          QJson::serialize(value.value<QRect>(), target); break;
    case QMetaType::QRectF:         QJson::serialize(value.value<QRectF>(), target); break;
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
