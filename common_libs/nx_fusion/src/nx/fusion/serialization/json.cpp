#include "json.h"

#include <QtCore/QJsonValue>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtCore/QtNumeric> //< for qIsFinite

#include <nx/utils/log/log.h>
#include <nx/utils/url.h>

#include "json_functions.h"

void QJsonDetail::serialize_json(
    const QJsonValue& value, QByteArray* outTarget, QJsonDocument::JsonFormat format)
{
    switch (value.type())
    {
        case QJsonValue::Null:
            *outTarget = "null";
            break;
        case QJsonValue::Bool:
            *outTarget = value.toBool() ? "true" : "false";
            break;
        case QJsonValue::Double:
        {
            double d = value.toDouble();
            if (qIsFinite(d))
            {
                *outTarget = QByteArray::number(value.toDouble(), 'g', 17);
            }
            else
            {
                *outTarget = "null"; //< +INF || -INF || NaN (see RFC4627#section2.4)
            }
            break;
        }
        case QJsonValue::String:
        {
            // We convert a string via QJsonDocument because escaping the special characters is not
            // that easy.
            QJsonArray array;
            array.push_back(value);
            QByteArray result = QJsonDocument(array).toJson(QJsonDocument::Compact);
            *outTarget = result.mid(1, result.size() - 2);
            break;
        }
        case QJsonValue::Array:
            *outTarget = QJsonDocument(value.toArray()).toJson(format);
            break;
        case QJsonValue::Object:
            *outTarget = QJsonDocument(value.toObject()).toJson(format);
            break;
        case QJsonValue::Undefined:
        default:
            outTarget->clear();
    }
}

QJsonValue::Type expected_json_type(const QByteArray& value)
{
    for (const char c: value)
    {
        switch (c)
        {
            case ' ':
            case '\t':
            case '\n':
            case '\v':
            case '\f':
            case '\r':
                continue; //< Whitespace characters.
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


bool QJsonDetail::deserialize_json(const QByteArray& value, QJsonValue* outTarget)
{
    switch (expected_json_type(value))
    {
        case QJsonValue::Null:
            if (value.trimmed() == "null")
            {
                *outTarget = QJsonValue();
                return true;
            }
            return false;
        case QJsonValue::Bool:
        {
            QByteArray trimmed = value.trimmed();
            if (trimmed == "true")
            {
                *outTarget = QJsonValue(true);
                return true;
            }
            if (trimmed == "false")
            {
                *outTarget = QJsonValue(false);
                return true;
            }
            return false;
        }
        case QJsonValue::Double:
        {
            bool ok;
            double parsed = value.trimmed().toDouble(&ok);
            if (ok)
                *outTarget = QJsonValue(parsed);
            return ok;
        }
        case QJsonValue::String:
        {
            // We convert string through QJsonDocument because handling escaping is not that easy.
            QByteArray arrayValue;
            arrayValue.reserve(value.size() + 2);
            arrayValue.append("[");
            arrayValue.append(value);
            arrayValue.append("]");

            QJsonParseError error;
            QJsonDocument document = QJsonDocument::fromJson(arrayValue, &error);
            if (error.error != QJsonParseError::NoError)
                return false;

            QJsonArray array = document.array();
            if (array.size() != 1)
                return false;

            *outTarget = array[0];
            return true;
        }
        case QJsonValue::Array:
        case QJsonValue::Object:
        {
            QJsonParseError error;
            QJsonDocument document = QJsonDocument::fromJson(value, &error);
            if (error.error != QJsonParseError::NoError)
                return false;

            if (document.isArray())
                *outTarget = document.array();
            else
                *outTarget = document.object();
            return true;
        }
        case QJsonValue::Undefined:
        default:
            return false;
    }
}

class QnJsonSerializerStorage: public QnSerializerStorage<QnJsonSerializer>
{
public:
    QnJsonSerializerStorage()
    {
        registerSerializer<QJsonValue>();
        registerSerializer<QJsonArray>();
        registerSerializer<QJsonObject>();

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
        registerSerializer<float>();
        registerSerializer<double>();

        registerSerializer<QString>();
        registerSerializer<QByteArray>();

        registerSerializer<QStringList>();
        registerSerializer<QList<QString>>();
        registerSerializer<QList<QByteArray>>();

        registerSerializer<QColor>();
        registerSerializer<QBrush>();
        registerSerializer<QSize>();
        registerSerializer<QSizeF>();
        registerSerializer<QRect>();
        registerSerializer<QRectF>();
        registerSerializer<QPoint>();
        registerSerializer<QPointF>();
        registerSerializer<QRegion>();
        registerSerializer<QVector2D>();
        registerSerializer<QVector3D>();
        registerSerializer<QVector4D>();
        registerSerializer<QnUuid>();
        registerSerializer<QUrl>();
        registerSerializer<nx::utils::Url>();
        registerSerializer<QFont>();

        registerSerializer<QnLatin1Array>();
    }
};

Q_GLOBAL_STATIC(QnJsonSerializerStorage, qn_jsonSerializerStorage_instance)

QnSerializerStorage<QnJsonSerializer>* QJsonDetail::StorageInstance::operator()() const
{
    return qn_jsonSerializerStorage_instance();
}

QJsonObject::const_iterator QJsonDetail::findField(
    const QJsonObject& jsonObject,
    const QString& fieldName,
    DeprecatedFieldNames* deprecatedFieldNames,
    const std::type_info& structTypeInfo)
{
    const QJsonObject::const_iterator pos = jsonObject.find(fieldName);
    if (pos != jsonObject.end())
        return pos;

    if (!deprecatedFieldNames)
    {
        NX_LOG(lit(
            "JSON field %2 of %1 not found (no deprecated names for this struct) in { %3 }")
            .arg(pointerTypeName(&structTypeInfo)).arg(fieldName)
            .arg(jsonObject.keys().join(", ")), cl_logDEBUG2);
        return jsonObject.end();
    }

    const QString deprecatedFieldName = deprecatedFieldNames->value(fieldName);
    if (deprecatedFieldName.isEmpty())
    {
        NX_LOG(lit(
            "JSON field %2 of %1 not found (no deprecated name for this field) in { %3 }")
            .arg(pointerTypeName(&structTypeInfo)).arg(fieldName)
            .arg(jsonObject.keys().join(", ")), cl_logDEBUG2);
        return jsonObject.end();
    }

    const QJsonObject::const_iterator deprecatedFieldPos = jsonObject.find(deprecatedFieldName);
    if (deprecatedFieldPos == jsonObject.end())
    {
        NX_LOG(lit(
            "JSON field %2 of %1 not found even as deprecated %3 in { %4 }")
            .arg(pointerTypeName(&structTypeInfo)).arg(fieldName).arg(deprecatedFieldName)
            .arg(jsonObject.keys().join(", ")), cl_logDEBUG2);
        return jsonObject.end();
    }
    return deprecatedFieldPos;
}
