#ifndef QN_SERIALIZATION_CSV_FUNCTIONS_H
#define QN_SERIALIZATION_CSV_FUNCTIONS_H

#include <map>
#include <vector>
#include <set>
#include <iterator> /* For std::iterator_traits. */

#include <boost/range/mutable_iterator.hpp>

#include <QtCore/QList>
#include <QtCore/QLinkedList>
#include <QtCore/QVector>
#include <QtCore/QSet>
#include <QtCore/QVarLengthArray>
#include <QtCore/QUuid>
#include <QtCore/QUrl>

#include "csv.h"


namespace QnCsvDetail {
    template<class Element, class Output>
    void serialize_collection_header(const Element &, QnCsvStreamWriter<Output> *stream, const QnCsv::field_tag &) {
        stream->writeField(lit("value"));
        stream->writeEndline();
    }

    template<class Element, class Output>
    void serialize_collection_header(const Element &value, QnCsvStreamWriter<Output> *stream, const QnCsv::record_tag &) {
        QnCsv::serialize_record_header(value, QString(), stream);
        stream->writeEndline();
    }

    template<class Element, class Output>
    void serialize_collection_element(const Element &value, QnCsvStreamWriter<Output> *stream, const QnCsv::field_tag &) {
        QnCsv::serialize_field(value, stream);
        stream->writeEndline();
    }

    template<class Element, class Output>
    void serialize_collection_element(const Element &value, QnCsvStreamWriter<Output> *stream, const QnCsv::record_tag &) {
        QnCsv::serialize_record(value, stream);
        stream->writeEndline();
    }

    template<class Collection, class Output>
    void serialize_collection(const Collection &value, QnCsvStreamWriter<Output> *stream) {
        typedef typename std::iterator_traits<typename boost::range_mutable_iterator<Collection>::type>::value_type value_type;
        typedef typename QnCsv::csv_category<value_type>::type category_type;

        serialize_collection_header(value_type(), stream, category_type());
        for(const auto &element: value)
            serialize_collection_element(element, stream, category_type());
    }

} // namespace QnCsvDetail


template<class Output>
void serialize_field(const QString &value, QnCsvStreamWriter<Output> *stream) {
    stream->writeField(value);
}

template<class Output>
void serialize_field(const QUuid &value, QnCsvStreamWriter<Output> *stream) {
    stream->writeLatin1Field(value.toByteArray());
}

template<class Output>
void serialize_field(const QUrl &value, QnCsvStreamWriter<Output> *stream) {
    stream->writeField(value.toString(QUrl::FullyEncoded));
}

template<class Output>
void serialize_field(const QByteArray &value, QnCsvStreamWriter<Output> *stream) {
    stream->writeLatin1Field(value.toBase64());
}


#define QN_DEFINE_NUMERIC_CSV_FIELD_SERIALIZATION_FUNCTIONS(TYPE, TYPE_GETTER, ... /* NUMBER_FORMAT */) \
template<class Output>                                                          \
void serialize_field(const TYPE &value, QnCsvStreamWriter<Output> *stream) {    \
    stream->writeLatin1Field(QByteArray::number(value, ##__VA_ARGS__));         \
}

QN_DEFINE_NUMERIC_CSV_FIELD_SERIALIZATION_FUNCTIONS(int,                toInt)
QN_DEFINE_NUMERIC_CSV_FIELD_SERIALIZATION_FUNCTIONS(unsigned int,       toUInt)
QN_DEFINE_NUMERIC_CSV_FIELD_SERIALIZATION_FUNCTIONS(long long,          toLongLong)
QN_DEFINE_NUMERIC_CSV_FIELD_SERIALIZATION_FUNCTIONS(unsigned long long, toULongLong)
QN_DEFINE_NUMERIC_CSV_FIELD_SERIALIZATION_FUNCTIONS(float,              toFloat, 'g', 9)
QN_DEFINE_NUMERIC_CSV_FIELD_SERIALIZATION_FUNCTIONS(double,             toDouble, 'g', 17)
#undef QN_DEFINE_NUMERIC_CSV_FIELD_SERIALIZATION_FUNCTIONS


#define QN_DEFINE_INTEGER_CONVERSION_CSV_FIELD_SERIALIZATION_FUNCTIONS(TYPE, TARGET_TYPE) \
template<class Output>                                                          \
void serialize_field(const TYPE &value, QnCsvStreamWriter<Output> *stream) {    \
    serialize_field(static_cast<TARGET_TYPE>(value), stream);                   \
}

QN_DEFINE_INTEGER_CONVERSION_CSV_FIELD_SERIALIZATION_FUNCTIONS(char,            int)
QN_DEFINE_INTEGER_CONVERSION_CSV_FIELD_SERIALIZATION_FUNCTIONS(signed char,     int) /* char, signed char and unsigned char are distinct types. */
QN_DEFINE_INTEGER_CONVERSION_CSV_FIELD_SERIALIZATION_FUNCTIONS(unsigned char,   int)
QN_DEFINE_INTEGER_CONVERSION_CSV_FIELD_SERIALIZATION_FUNCTIONS(short,           int)
QN_DEFINE_INTEGER_CONVERSION_CSV_FIELD_SERIALIZATION_FUNCTIONS(unsigned short,  int)
QN_DEFINE_INTEGER_CONVERSION_CSV_FIELD_SERIALIZATION_FUNCTIONS(long,            long long)
QN_DEFINE_INTEGER_CONVERSION_CSV_FIELD_SERIALIZATION_FUNCTIONS(unsigned long,   long long)
#undef QN_DEFINE_INTEGER_CONVERSION_CSV_FIELD_SERIALIZATION_FUNCTIONS


#ifndef Q_MOC_RUN
#define QN_DEFINE_COLLECTION_CSV_SERIALIZATION_FUNCTIONS(TYPE, TPL_DEF, TPL_ARG) \
template<BOOST_PP_TUPLE_ENUM(TPL_DEF), class Output>                            \
void serialize_document(const TYPE<BOOST_PP_TUPLE_ENUM(TPL_ARG)> &value, QnCsvStreamWriter<Output> *stream) { \
    QnCsvDetail::serialize_collection(value, stream);                           \
}

QN_DEFINE_COLLECTION_CSV_SERIALIZATION_FUNCTIONS(QSet, (class T), (T));
QN_DEFINE_COLLECTION_CSV_SERIALIZATION_FUNCTIONS(QList, (class T), (T));
QN_DEFINE_COLLECTION_CSV_SERIALIZATION_FUNCTIONS(QLinkedList, (class T), (T));
QN_DEFINE_COLLECTION_CSV_SERIALIZATION_FUNCTIONS(QVector, (class T), (T));
QN_DEFINE_COLLECTION_CSV_SERIALIZATION_FUNCTIONS(QMap, (class Key, class T), (Key, T));
QN_DEFINE_COLLECTION_CSV_SERIALIZATION_FUNCTIONS(QVarLengthArray, (class T, int N), (T, N));
QN_DEFINE_COLLECTION_CSV_SERIALIZATION_FUNCTIONS(std::vector, (class T, class Allocator), (T, Allocator));
QN_DEFINE_COLLECTION_CSV_SERIALIZATION_FUNCTIONS(std::set, (class Key, class Predicate, class Allocator), (Key, Predicate, Allocator));
QN_DEFINE_COLLECTION_CSV_SERIALIZATION_FUNCTIONS(std::map, (class Key, class T, class Predicate, class Allocator), (Key, T, Predicate, Allocator)); // TODO: #Elric should not be here
#undef QN_DEFINE_COLLECTION_CSV_SERIALIZATION_FUNCTIONS
#endif // Q_MOC_RUN


#endif // QN_SERIALIZATION_CSV_FUNCTIONS_H
