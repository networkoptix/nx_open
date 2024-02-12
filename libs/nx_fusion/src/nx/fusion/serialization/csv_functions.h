// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_SERIALIZATION_CSV_FUNCTIONS_H
#define QN_SERIALIZATION_CSV_FUNCTIONS_H

#include <iterator> /* For std::iterator_traits. */
#include <map>
#include <set>
#include <type_traits>
#include <vector>

#ifndef Q_MOC_RUN
    #include <boost/preprocessor/tuple/enum.hpp>
    #include <boost/range/mutable_iterator.hpp>
#endif

#include <QtCore/QUrl>

#include <nx/reflect/to_string.h>
#include <nx/utils/latin1_array.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>

#include "collection_fwd.h"
#include "csv.h"
#include "csv_from_json.h"
#include "csv_macros.h"
#include "enum.h"
#include "lexical.h"

namespace QnCsvDetail {
    template<class Collection, class Output>
    void serialize_collection(const Collection &value, QnCsvStreamWriter<Output> *stream) {
        typedef typename std::iterator_traits<typename boost::range_mutable_iterator<Collection>::type>::value_type value_type;

        QnCsv::serialize_header<value_type>(QString(), stream);
        stream->writeEndline();

        for(const auto &element: value) {
            QnCsv::serialize(element, stream);
            stream->writeEndline();
        }
    }

} // namespace QnCsvDetail


QN_DECLARE_CSV_TYPE_CATEGORY(QString, QnCsv::field_tag)
QN_DECLARE_CSV_TYPE_CATEGORY(QByteArray, QnCsv::field_tag)
QN_DECLARE_CSV_TYPE_CATEGORY(QnLatin1Array, QnCsv::field_tag)

template<class Output>
void serialize(const bool &value, QnCsvStreamWriter<Output> *stream) {
    stream->writeUtf8Field(value ? "true" : "false");
}

template<class Output>
void serialize(const QString &value, QnCsvStreamWriter<Output> *stream) {
    stream->writeField(value);
}

template<class Output>
void serialize(const nx::Uuid &value, QnCsvStreamWriter<Output> *stream) {
    stream->writeUtf8Field(value.toByteArray());
}

template<class Output>
void serialize(const QUrl &value, QnCsvStreamWriter<Output> *stream) {
    stream->writeUtf8Field(value.toEncoded());
}

template<class Output>
void serialize(const nx::utils::Url& value, QnCsvStreamWriter<Output> *stream) {
    stream->writeUtf8Field(value.toEncoded());
}

template<class Output>
void serialize(const QByteArray &value, QnCsvStreamWriter<Output> *stream) {
    stream->writeUtf8Field(value.toBase64());
}

template<class Output>
void serialize(const QnLatin1Array &value, QnCsvStreamWriter<Output> *stream) {
    stream->writeUtf8Field(value);
}


#define QN_DEFINE_NUMERIC_CSV_SERIALIZATION_FUNCTIONS(TYPE, TYPE_GETTER, ... /* NUMBER_FORMAT */) \
template<class Output>                                                          \
void serialize(const TYPE &value, QnCsvStreamWriter<Output> *stream) {          \
    stream->writeUtf8Field(QByteArray::number(value, ##__VA_ARGS__));           \
}

QN_DEFINE_NUMERIC_CSV_SERIALIZATION_FUNCTIONS(int,                toInt)
QN_DEFINE_NUMERIC_CSV_SERIALIZATION_FUNCTIONS(unsigned int,       toUInt)
QN_DEFINE_NUMERIC_CSV_SERIALIZATION_FUNCTIONS(long long,          toLongLong)
QN_DEFINE_NUMERIC_CSV_SERIALIZATION_FUNCTIONS(unsigned long long, toULongLong)
QN_DEFINE_NUMERIC_CSV_SERIALIZATION_FUNCTIONS(float,              toFloat, 'g', 9)
QN_DEFINE_NUMERIC_CSV_SERIALIZATION_FUNCTIONS(double,             toDouble, 'g', 17)
#undef QN_DEFINE_NUMERIC_CSV_SERIALIZATION_FUNCTIONS


#define QN_DEFINE_INTEGER_CONVERSION_CSV_SERIALIZATION_FUNCTIONS(TYPE, TARGET_TYPE) \
template<class Output>                                                          \
void serialize(const TYPE &value, QnCsvStreamWriter<Output> *stream) {          \
    serialize(static_cast<TARGET_TYPE>(value), stream);                         \
}

QN_DEFINE_INTEGER_CONVERSION_CSV_SERIALIZATION_FUNCTIONS(char,            int)
QN_DEFINE_INTEGER_CONVERSION_CSV_SERIALIZATION_FUNCTIONS(signed char,     int) /* char, signed char and unsigned char are distinct types. */
QN_DEFINE_INTEGER_CONVERSION_CSV_SERIALIZATION_FUNCTIONS(unsigned char,   int)
QN_DEFINE_INTEGER_CONVERSION_CSV_SERIALIZATION_FUNCTIONS(short,           int)
QN_DEFINE_INTEGER_CONVERSION_CSV_SERIALIZATION_FUNCTIONS(unsigned short,  int)
QN_DEFINE_INTEGER_CONVERSION_CSV_SERIALIZATION_FUNCTIONS(long,            long long)
QN_DEFINE_INTEGER_CONVERSION_CSV_SERIALIZATION_FUNCTIONS(unsigned long,   long long)
#undef QN_DEFINE_INTEGER_CONVERSION_CSV_SERIALIZATION_FUNCTIONS

template<class Output>
void serialize(const std::chrono::milliseconds &value, QnCsvStreamWriter<Output> *stream)
{
    serialize(qint64(value.count()), stream);                         \
}

template<typename Clock, typename Duration, typename Output>
void serialize(
    const std::chrono::time_point<Clock, Duration>& timePoint, QnCsvStreamWriter<Output> *stream)
{
    using namespace std::chrono;
    serialize(duration_cast<milliseconds>(timePoint.time_since_epoch()).count(), stream);
}


#ifndef Q_MOC_RUN
#define QN_DEFINE_COLLECTION_CSV_SERIALIZATION_FUNCTIONS(TYPE, TPL_DEF, TPL_ARG) \
template<BOOST_PP_TUPLE_ENUM(TPL_DEF), class Output>                            \
void serialize(const TYPE<BOOST_PP_TUPLE_ENUM(TPL_ARG)> &value, QnCsvStreamWriter<Output> *stream) { \
    QnCsvDetail::serialize_collection(value, stream);                           \
}

QN_DEFINE_COLLECTION_CSV_SERIALIZATION_FUNCTIONS(QSet, (class T), (T));
QN_DEFINE_COLLECTION_CSV_SERIALIZATION_FUNCTIONS(QList, (class T), (T));
QN_DEFINE_COLLECTION_CSV_SERIALIZATION_FUNCTIONS(QLinkedList, (class T), (T));
QN_DEFINE_COLLECTION_CSV_SERIALIZATION_FUNCTIONS(QMap, (class Key, class T), (Key, T));
QN_DEFINE_COLLECTION_CSV_SERIALIZATION_FUNCTIONS(QVarLengthArray, (class T, qsizetype N), (T, N));
QN_DEFINE_COLLECTION_CSV_SERIALIZATION_FUNCTIONS(std::vector, (class T, class Allocator), (T, Allocator));
QN_DEFINE_COLLECTION_CSV_SERIALIZATION_FUNCTIONS(std::set, (class Key, class Predicate, class Allocator), (Key, Predicate, Allocator));
#undef QN_DEFINE_COLLECTION_CSV_SERIALIZATION_FUNCTIONS
#endif // Q_MOC_RUN

template<typename T, typename Output>
void serialize(
    const T& value,
    QnCsvStreamWriter<Output>* stream,
    typename std::enable_if<QnSerialization::IsEnumOrFlags<T>::value>::type* = nullptr)
{
    if constexpr (QnSerialization::IsInstrumentedEnumOrFlags<T>::value)
    {
        stream->writeField(QString::fromStdString(nx::reflect::toString(value)));
    }
    else
    {
        // All enums are by default lexically serialized.
        stream->writeField(QnLexical::serialized(value));
    }
}

template<class Output>
inline void serialize(const QJsonValue& value, QnCsvStreamWriter<Output>* stream)
{
    QnCsvDetail::Fields fields;
    bool needComma = false;
    if (value.isArray() || value.isObject())
    {
        QnCsvDetail::collectHeader(value, &fields);
        QnCsvDetail::writeHeader({}, fields, stream, &needComma);
        stream->writeEndline();
    }
    switch (value.type())
    {
        case QJsonValue::Array:
            QnCsvDetail::serialize(value.toArray(), fields, stream);
            break;
        case QJsonValue::Bool:
            serialize(value.toBool(), stream);
            break;
        case QJsonValue::Double:
            serialize(value.toDouble(), stream);
            break;
        case QJsonValue::Object:
            needComma = false;
            QnCsvDetail::serialize(value.toObject(), fields, stream, &needComma);
            break;
        default:
            serialize(QnLexical::serialized(value), stream);
            break;
    }
}

#endif // QN_SERIALIZATION_CSV_FUNCTIONS_H
