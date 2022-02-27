// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_SERIALIZATION_LEXICAL_FUNCTIONS_H
#define QN_SERIALIZATION_LEXICAL_FUNCTIONS_H

#include <chrono>

#include <nx/utils/uuid.h>
#include <QtCore/QUrl>
#include <QtCore/QBitArray>
#include <QtGui/QColor>

#include "lexical.h"

#include "nx/utils/latin1_array.h"
#include "nx/fusion/fusion/fusion_fwd.h"
#include <nx/utils/url.h>

namespace QnLexicalDetail {
    template<class T, class Temporary>
    void serialize_integer(const T &value, QString *target) {
        *target = QString::number(static_cast<Temporary>(value));
    }

    template<class T, class Temporary>
    bool deserialize_integer(const QString &value, T *target) {
        Temporary tmp;
        if(!QnLexical::deserialize(value, &tmp))
            return false;
        if(tmp < static_cast<Temporary>(std::numeric_limits<T>::min()) || tmp > static_cast<Temporary>(std::numeric_limits<T>::max()))
            return false;

        *target = static_cast<T>(tmp);
        return true;
    }


    template<class T>
    void serialize_numeric_enum(const T &value, QString *target) {
        QnLexical::serialize(static_cast<qint32>(value), target);
    }

    template<class T>
    bool deserialize_numeric_enum(const QString &value, T *target) {
        qint32 tmp;
        if(!QnLexical::deserialize(value, &tmp))
            return false;
        *target = static_cast<T>(tmp);
        return true;
    }
} // namespace QnLexicalDetail


/* Free-standing (de)serialization functions are picked up via ADL by
 * the actual implementation. Feel free to add them for your own types. */


inline void serialize(const QString &value, QString *target) {
    *target = value;
}

inline bool deserialize(const QString &value, QString *target) {
    *target = value;
    return true;
}


#define QN_DEFINE_DIRECT_LEXICAL_SERIALIZATION_FUNCTIONS(TYPE, TYPE_GETTER, ... /* NUMBER_FORMAT */) \
inline void serialize(const TYPE &value, QString *target) {                     \
    *target = QString::number(value, ##__VA_ARGS__);                            \
}                                                                               \
                                                                                \
inline bool deserialize(const QString &value, TYPE *target) {                   \
    bool ok = false;                                                            \
    TYPE result = value.TYPE_GETTER(&ok);                                       \
    if(ok)                                                                      \
        *target = result;                                                       \
    return ok;                                                                  \
}

QN_DEFINE_DIRECT_LEXICAL_SERIALIZATION_FUNCTIONS(int,                   toInt)
QN_DEFINE_DIRECT_LEXICAL_SERIALIZATION_FUNCTIONS(unsigned int,          toUInt)
QN_DEFINE_DIRECT_LEXICAL_SERIALIZATION_FUNCTIONS(long,                  toLong)
QN_DEFINE_DIRECT_LEXICAL_SERIALIZATION_FUNCTIONS(unsigned long,         toULong)
QN_DEFINE_DIRECT_LEXICAL_SERIALIZATION_FUNCTIONS(long long,             toLongLong)
QN_DEFINE_DIRECT_LEXICAL_SERIALIZATION_FUNCTIONS(unsigned long long,    toULongLong)
QN_DEFINE_DIRECT_LEXICAL_SERIALIZATION_FUNCTIONS(float,                 toFloat, 'g', 9)
QN_DEFINE_DIRECT_LEXICAL_SERIALIZATION_FUNCTIONS(double,                toDouble, 'g', 17)
#undef QN_DEFINE_DIRECT_LEXICAL_SERIALIZATION_FUNCTIONS


#define QN_DEFINE_INTEGER_CONVERSION_LEXICAL_SERIALIZATION_FUNCTIONS(TYPE)      \
inline void serialize(const TYPE &value, QString *target) {                     \
    QnLexicalDetail::serialize_integer<TYPE, int>(value, target);               \
}                                                                               \
                                                                                \
inline bool deserialize(const QString &value, TYPE *target) {                   \
    return QnLexicalDetail::deserialize_integer<TYPE, int>(value, target);      \
}

QN_DEFINE_INTEGER_CONVERSION_LEXICAL_SERIALIZATION_FUNCTIONS(char)
QN_DEFINE_INTEGER_CONVERSION_LEXICAL_SERIALIZATION_FUNCTIONS(signed char) /* char, signed char and unsigned char are distinct types. */
QN_DEFINE_INTEGER_CONVERSION_LEXICAL_SERIALIZATION_FUNCTIONS(unsigned char)
QN_DEFINE_INTEGER_CONVERSION_LEXICAL_SERIALIZATION_FUNCTIONS(short)
QN_DEFINE_INTEGER_CONVERSION_LEXICAL_SERIALIZATION_FUNCTIONS(unsigned short)
#undef QN_DEFINE_INTEGER_CONVERSION_LEXICAL_SERIALIZATION_FUNCTIONS

inline void serialize(const std::chrono::seconds& value, QString* target)
{
    *target = QString::number(value.count());
}

inline void serialize(const std::chrono::milliseconds& value, QString* target)
{
    *target = QString::number(value.count());
}

inline void serialize(const std::chrono::microseconds& value, QString* target)
{
    *target = QString::number(value.count());
}

template<typename Clock, typename Duration>
void serialize(const std::chrono::time_point<Clock, Duration>& timePoint, QString* target)
{
    using namespace std::chrono;
    *target = QString::number(duration_cast<milliseconds>(timePoint.time_since_epoch()).count());
}

inline bool deserialize(const QString &value, std::chrono::seconds* target)
{
    qint64 tmp;
    if(!QnLexical::deserialize(value, &tmp))
        return false;
    *target = std::chrono::seconds(tmp);
    return true;
}

inline bool deserialize(const QString &value, std::chrono::milliseconds* target)
{
    qint64 tmp;
    if(!QnLexical::deserialize(value, &tmp))
        return false;
    *target = std::chrono::milliseconds(tmp);
    return true;
}

inline bool deserialize(const QString &value, std::chrono::microseconds* target)
{
    qint64 tmp;
    if(!QnLexical::deserialize(value, &tmp))
        return false;
    *target = std::chrono::microseconds(tmp);
    return true;
}

template<typename Clock, typename Duration>
bool deserialize(const QString& value, std::chrono::time_point<Clock, Duration>* target)
{
    qint64 tmp;
    if (!QnLexical::deserialize(value, &tmp))
        return false;
    *target = std::chrono::time_point<Clock, Duration>(std::chrono::milliseconds(tmp));
    return true;
}

inline void serialize(const std::string& value, QString* target)
{
    *target = QString::fromStdString(value);
}

inline bool deserialize(const QString &value, std::string* target)
{
    *target = value.toStdString(); //< We don't check for errors...
    return true;
}

inline void serialize(const QnLatin1Array &value, QString *target) {
    *target = QString::fromLatin1(value);
}

inline bool deserialize(const QString &value, QnLatin1Array *target) {
    *target = value.toLatin1(); /* We don't check for errors... */
    return true;
}

inline void serialize(const QByteArray &value, QString *target) {
    *target = QLatin1String(value.toBase64());
}

inline bool deserialize(const QString &value, QByteArray *target) {
    *target = QByteArray::fromBase64(value.toLatin1());
    return true;
}

inline void serialize(const QJsonValue &value, QString *target) {
    *target = value.toString();
}

inline bool deserialize(const QString &value, QJsonValue *target) {
    *target = value;
    return true;
}

NX_FUSION_API bool serialize(const QBitArray& value, QString* target);
NX_FUSION_API bool deserialize(const QString& value, QBitArray* target);

NX_FUSION_API bool serialize(const QRect& value, QString* target);
NX_FUSION_API bool deserialize(const QString& value, QRect* target);
NX_FUSION_API bool serialize(const QRectF& value, QString* target);
NX_FUSION_API bool deserialize(const QString& value, QRectF* target);

NX_FUSION_API bool serialize(const QJsonValue::Type& value, QString* target);
NX_FUSION_API bool deserialize(const QString& value, QJsonValue::Type* target);

QN_FUSION_DECLARE_FUNCTIONS(bool, (lexical), NX_FUSION_API)
QN_FUSION_DECLARE_FUNCTIONS(QnUuid, (lexical), NX_FUSION_API)
QN_FUSION_DECLARE_FUNCTIONS(QUrl, (lexical), NX_FUSION_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::utils::Url, (lexical), NX_FUSION_API)
QN_FUSION_DECLARE_FUNCTIONS(QColor, (lexical), NX_FUSION_API)

#endif // QN_SERIALIZATION_LEXICAL_FUNCTIONS_H
