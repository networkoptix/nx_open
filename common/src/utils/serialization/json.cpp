#include "json.h"

#include <QtCore/QJsonValue>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtCore/QtNumeric> /* For qIsFinite. */

#include "json_functions.h"

void QJsonDetail::serialize_json(const QJsonValue &value, QByteArray *target, QJsonDocument::JsonFormat format) {
    switch (value.type()) {
    case QJsonValue::Null:
        *target = "null";
        break;
    case QJsonValue::Bool:
        *target = value.toBool() ? "true" : "false";
        break;
    case QJsonValue::Double: {
        double d = value.toDouble();
        if(qIsFinite(d)) {
            *target = QByteArray::number(value.toDouble(), 'g', 17);
        } else {
            *target = "null"; /* +INF || -INF || NaN (see RFC4627#section2.4) */
        }
        break;
    }
    case QJsonValue::String:
        target->clear();
        target->append('\"');
        target->append(value.toString().toUtf8());
        target->append('\"');
        break;
    case QJsonValue::Array:
        *target = QJsonDocument(value.toArray()).toJson(format);
        break;
    case QJsonValue::Object:
        *target = QJsonDocument(value.toObject()).toJson(format);
        break;
    case QJsonValue::Undefined:
    default:
        target->clear();
        break;
    }
}

QJsonValue::Type expected_json_type(const QByteArray &value) {
    foreach(const char c, value) {
        switch (c) {
        case ' ':
        case '\t':
        case '\n':
        case '\v':
        case '\f':
        case '\r':
            continue; /* Whitespace characters. */
        case 'n':
            return QJsonValue::Null;
        case 't':
        case 'f':
            return QJsonValue::Bool;
        case '+':
        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '.':
            return QJsonValue::Double;
        case '\"':
            return QJsonValue::String;
        case '[':
            return QJsonValue::Array;
        case '{':
            return QJsonValue::Object;
        default:
            return QJsonValue::Undefined;
        }
    }

    return QJsonValue::Undefined;
}


bool QJsonDetail::deserialize_json(const QByteArray &value, QJsonValue *target) {
    switch (expected_json_type(value))
    case QJsonValue::Null: {
        if(value.trimmed() == "null") {
            *target = QJsonValue();
            return true;
        } else {
            return false;
        }
    case QJsonValue::Bool: {
        QByteArray trimmed = value.trimmed();
        if(trimmed == "true") {
            *target = QJsonValue(true);
            return true;
        } else if(trimmed == "false") {
            *target = QJsonValue(false);
            return true;
        } else {
            return false;
        }
    }
    case QJsonValue::Double: {
        bool ok;
        double parsed = value.trimmed().toDouble(&ok);
        if(ok)
            *target = QJsonValue(parsed);
        return ok;
    }
    case QJsonValue::String: {
        QByteArray trimmed = value.trimmed();
        if(trimmed.startsWith('\"') && trimmed.endsWith('\"')) {
            *target = QJsonValue(QString::fromUtf8(trimmed.constData() + 1, trimmed.size() - 2));
            return true;
        } else {
            return false;
        }
    }
    case QJsonValue::Array:
    case QJsonValue::Object: {
        QJsonParseError error;
        QJsonDocument document = QJsonDocument::fromJson(value, &error);
        if(error.error != QJsonParseError::NoError)
            return false;

        if(document.isArray()) {
            *target = document.array();
        } else {
            *target = document.object();
        }
        return true;
    }
    case QJsonValue::Undefined:
    default:
        return false;
    }
}

class QnJsonSerializerStorage: public QnSerializerStorage<QnJsonSerializer> {
public:
    QnJsonSerializerStorage() {
        registerSerializer<QString>();
        registerSerializer<double>();
        registerSerializer<bool>();
        registerSerializer<char>();
        registerSerializer<signed char>();
        registerSerializer<unsigned char>();
        registerSerializer<short>();
        registerSerializer<unsigned short>();
        registerSerializer<int>();
        registerSerializer<unsigned int>();
        registerSerializer<long>();
        registerSerializer<unsigned long>();
        registerSerializer<long long>();
        registerSerializer<unsigned long long>();

        registerSerializer<QColor>();
        registerSerializer<QBrush>();
        registerSerializer<QFont>();
    }
};

Q_GLOBAL_STATIC(QnJsonSerializerStorage, qn_jsonSerializerStorage_instance)

QnSerializerStorage<QnJsonSerializer> *QJsonDetail::StorageInstance::operator()() const {
    return qn_jsonSerializerStorage_instance();
}

