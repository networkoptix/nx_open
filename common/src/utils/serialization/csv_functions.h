#ifndef QN_SERIALIZATION_CSV_FUNCTIONS_H
#define QN_SERIALIZATION_CSV_FUNCTIONS_H

#include "csv.h"

#define QN_DEFINE_NUMERIC_CSV_FIELD_SERIALIZATION_FUNCTIONS(TYPE, TYPE_GETTER, ... /* NUMBER_FORMAT */) \
template<class Output>                                                          \
void serialize_field(const TYPE &value, QnCsvStreamWriter<Output> *stream) {    \
    stream->writeLatin1Field(QByteArray::number(value, ##__VA_ARGS__));         \
}
                                                 
QN_DEFINE_NUMERIC_CSV_FIELD_SERIALIZATION_FUNCTIONS(int,                toInt)
QN_DEFINE_NUMERIC_CSV_FIELD_SERIALIZATION_FUNCTIONS(unsigned int,       toUInt)
QN_DEFINE_NUMERIC_CSV_FIELD_SERIALIZATION_FUNCTIONS(long,               toLong)
QN_DEFINE_NUMERIC_CSV_FIELD_SERIALIZATION_FUNCTIONS(unsigned long,      toULong)
QN_DEFINE_NUMERIC_CSV_FIELD_SERIALIZATION_FUNCTIONS(long long,          toLongLong)
QN_DEFINE_NUMERIC_CSV_FIELD_SERIALIZATION_FUNCTIONS(unsigned long long, toULongLong)
QN_DEFINE_NUMERIC_CSV_FIELD_SERIALIZATION_FUNCTIONS(float,              toFloat, 'g', 9)
QN_DEFINE_NUMERIC_CSV_FIELD_SERIALIZATION_FUNCTIONS(double,             toDouble, 'g', 17)
#undef QN_DEFINE_NUMERIC_CSV_FIELD_SERIALIZATION_FUNCTIONS


#define QN_DEFINE_INTEGER_CONVERSION_CSV_FIELD_SERIALIZATION_FUNCTIONS(TYPE)    \
template<class Output>                                                          \
void serialize_field(const TYPE &value, QnCsvStreamWriter<Output> *stream) {    \
    serialize_field(static_cast<int>(value), target);                           \
}

QN_DEFINE_INTEGER_CONVERSION_CSV_FIELD_SERIALIZATION_FUNCTIONS(char)
QN_DEFINE_INTEGER_CONVERSION_CSV_FIELD_SERIALIZATION_FUNCTIONS(signed char) /* char, signed char and unsigned char are distinct types. */
QN_DEFINE_INTEGER_CONVERSION_CSV_FIELD_SERIALIZATION_FUNCTIONS(unsigned char)
QN_DEFINE_INTEGER_CONVERSION_CSV_FIELD_SERIALIZATION_FUNCTIONS(short)
QN_DEFINE_INTEGER_CONVERSION_CSV_FIELD_SERIALIZATION_FUNCTIONS(unsigned short)
#undef QN_DEFINE_INTEGER_CONVERSION_CSV_FIELD_SERIALIZATION_FUNCTIONS


template<class Output>
void serialize(const QString &value, QnCsvStreamWriter<Output> *stream) {
    stream->writeField(value);
}

template<class Collection, class Output>
void serialize_document(const Collection &value, QnCsvStreamWriter<Output> *stream) {
    QnCsv::serialize_record_header(value, QString(), stream);
    
    for(const auto &element: value)
        QnCsv::serialize_record(element, stream);
}

#endif // QN_SERIALIZATION_CSV_FUNCTIONS_H
