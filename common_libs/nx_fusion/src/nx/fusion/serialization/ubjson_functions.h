#ifndef QN_UBJSON_FUNCTIONS_H
#define QN_UBJSON_FUNCTIONS_H

#include <type_traits> /* For std::enable_if. */
#include <array>
#include <vector>
#include <set>
#include <map>
#include <array>

#ifndef Q_MOC_RUN
#include <boost/preprocessor/tuple/enum.hpp>
#include <boost/preprocessor/cat.hpp>
#endif

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QUrl>

#include <nx/utils/collection.h>
#include <nx/utils/latin1_array.h>
#include <nx/utils/uuid.h>
#include <nx/utils/url.h>

#include "collection_fwd.h"
#include "ubjson.h"
#include "ubjson_macros.h"
#include "enum.h"


namespace QnUbjsonDetail {

    template<class Element, class Output, class Tag>
    void serialize_collection_element(const Element &element, QnUbjsonWriter<Output> *stream, const Tag &) {
        QnUbjson::serialize(element, stream);
    }

    template<class Element, class Output>
    void serialize_collection_element(const Element &element, QnUbjsonWriter<Output> *stream, const QnCollection::map_tag &) {
        stream->writeArrayStart();
        QnUbjson::serialize(element.first, stream);
        QnUbjson::serialize(element.second, stream);
        stream->writeArrayEnd();
    }

    template<class Collection, class Output>
    void serialize_collection(const Collection &value, QnUbjsonWriter<Output> *stream) {
        stream->writeArrayStart(static_cast<int>(value.size()));

        for(auto pos = boost::begin(value); pos != boost::end(value); ++pos)
            serialize_collection_element(*pos, stream, typename QnCollection::collection_category<Collection>::type());

        stream->writeArrayEnd();
    }

    template<class Collection, class Input, class Element>
    bool deserialize_collection_element(QnUbjsonReader<Input> *stream, Collection *target, const Element *, const QnCollection::list_tag &) {
        return QnUbjson::deserialize(stream, &*QnCollection::insert(*target, boost::end(*target), Element()));
    }

    template<class Collection, class Input, class Element>
    bool deserialize_collection_element(QnUbjsonReader<Input> *stream, Collection *target, const Element *, const QnCollection::set_tag &) {
        Element element;
        if(!QnUbjson::deserialize(stream, &element))
            return false;

        QnCollection::insert(*target, boost::end(*target), std::move(element));
        return true;
    }

    template<class Collection, class Input, class Element>
    bool deserialize_collection_element(QnUbjsonReader<Input> *stream, Collection *target, const Element *, const QnCollection::map_tag &) {
        if(!stream->readArrayStart())
            return false;

        typename Collection::key_type key;
        if(!QnUbjson::deserialize(stream, &key))
            return false;

        if(!QnUbjson::deserialize(stream, &(*target)[key]))
            return false;

        if(!stream->readArrayEnd())
            return false;

        return true;
    }

    template<class Collection, class Input>
    bool deserialize_collection(QnUbjsonReader<Input> *stream, Collection *target) {
        typedef typename std::iterator_traits<typename boost::range_mutable_iterator<Collection>::type>::value_type value_type;

        int size;
        if(!stream->readArrayStart(&size))
            return false;

        QnCollection::clear(*target);
        if(size >= 0)
            QnCollection::reserve(*target, size);

        while(true) {
            QnUbjson::Marker marker = stream->peekMarker();
            if(marker == QnUbjson::ArrayEndMarker)
                break;

            if(!deserialize_collection_element(stream, target, static_cast<const value_type *>(NULL), typename QnCollection::collection_category<Collection>::type()))
                return false;
        }

        if(!stream->readArrayEnd())
            return false;

        return true;
    }

    template<class Map, class Output>
    void serialize_string_map(const Map &value, QnUbjsonWriter<Output> *stream) {
        stream->writeObjectStart(static_cast<int>(value.size()));

        for(auto pos = boost::begin(value); pos != boost::end(value); pos++) {
            QnUbjson::serialize(pos->first, stream);
            QnUbjson::serialize(pos->second, stream);
        }

        stream->writeObjectEnd();
    }

    template<class Map, class Input>
    bool deserialize_string_map(QnUbjsonReader<Input> *stream, Map *target) {
        int size;
        if(!stream->readObjectStart(&size))
            return false;

        QnCollection::clear(*target);
        if(size >= 0)
            QnCollection::reserve(*target, size);

        while(true) {
            QnUbjson::Marker marker = stream->peekMarker();
            if(marker == QnUbjson::ObjectEndMarker)
                break;

            QString key;
            if(!QnUbjson::deserialize(stream, &key))
                return false;

            if(!QnUbjson::deserialize(stream, &(*target)[key]))
                return false;
        }

        if(!stream->readObjectEnd())
            return false;

        return true;
    }

    template<class T, class Temporary, class Output>
    void serialize_integer(const T &value, QnUbjsonWriter<Output> *stream) {
        QnUbjson::serialize(static_cast<Temporary>(value), stream);
    }

    template<class T, class Temporary, class Input>
    bool deserialize_integer(QnUbjsonReader<Input> *stream, T *target) {
        Temporary tmp;
        if(!QnUbjson::deserialize(stream, &tmp))
            return false;
        *target = static_cast<T>(tmp);
        return true;
    }

} // QnUbjsonDetail


#ifndef Q_MOC_RUN
#define QN_DEFINE_DIRECT_UBJSON_SERIALIZATION_FUNCTIONS(TYPE, READ_METHOD, WRITE_METHOD) \
template<class Output>                                                          \
void serialize(const TYPE &value, QnUbjsonWriter<Output> *stream) {             \
    stream->WRITE_METHOD(value);                                                \
}                                                                               \
                                                                                \
template<class Input>                                                           \
bool deserialize(QnUbjsonReader<Input> *stream, TYPE *target) {                 \
    return stream->READ_METHOD(target);                                         \
}

QN_DEFINE_DIRECT_UBJSON_SERIALIZATION_FUNCTIONS(bool,          readBool,       writeBool)
QN_DEFINE_DIRECT_UBJSON_SERIALIZATION_FUNCTIONS(quint8,        readUInt8,      writeUInt8)
QN_DEFINE_DIRECT_UBJSON_SERIALIZATION_FUNCTIONS(qint8,         readInt8,       writeInt8)
QN_DEFINE_DIRECT_UBJSON_SERIALIZATION_FUNCTIONS(quint16,       readUInt16,     writeUInt16)
QN_DEFINE_DIRECT_UBJSON_SERIALIZATION_FUNCTIONS(qint16,        readInt16,      writeInt16)
QN_DEFINE_DIRECT_UBJSON_SERIALIZATION_FUNCTIONS(qint32,        readInt32,      writeInt32)
QN_DEFINE_DIRECT_UBJSON_SERIALIZATION_FUNCTIONS(qint64,        readInt64,      writeInt64)
QN_DEFINE_DIRECT_UBJSON_SERIALIZATION_FUNCTIONS(quint64,       readUInt64,     writeUInt64)
QN_DEFINE_DIRECT_UBJSON_SERIALIZATION_FUNCTIONS(float,         readFloat,      writeFloat)
QN_DEFINE_DIRECT_UBJSON_SERIALIZATION_FUNCTIONS(double,        readDouble,     writeDouble)
QN_DEFINE_DIRECT_UBJSON_SERIALIZATION_FUNCTIONS(QString,       readUtf8String, writeUtf8String)
QN_DEFINE_DIRECT_UBJSON_SERIALIZATION_FUNCTIONS(QnLatin1Array, readUtf8String, writeUtf8String)
QN_DEFINE_DIRECT_UBJSON_SERIALIZATION_FUNCTIONS(QByteArray,    readBinaryData, writeBinaryData)
#undef QN_DEFINE_DIRECT_UBJSON_SERIALIZATION_FUNCTIONS


#define QN_DEFINE_INTEGER_CONVERSION_UBJSON_SERIALIZATION_FUNCTIONS(TYPE, TARGET_TYPE) \
template<class Output>                                                          \
void serialize(const TYPE &value, QnUbjsonWriter<Output> *stream) {             \
    QnUbjsonDetail::serialize_integer<TYPE, TARGET_TYPE>(value, stream);        \
}                                                                               \
                                                                                \
template<class Input>                                                           \
bool deserialize(QnUbjsonReader<Input> *stream, TYPE *target) {                 \
    return QnUbjsonDetail::deserialize_integer<TYPE, TARGET_TYPE>(stream, target); \
}

//QN_DEFINE_INTEGER_CONVERSION_UBJSON_SERIALIZATION_FUNCTIONS(quint16, qint16)
//QN_DEFINE_INTEGER_CONVERSION_UBJSON_SERIALIZATION_FUNCTIONS(quint32, qint32) // This is not semantically correct, so it's commented out.
//QN_DEFINE_INTEGER_CONVERSION_UBJSON_SERIALIZATION_FUNCTIONS(quint64, qint64)
#undef QN_DEFINE_INTEGER_CONVERSION_UBJSON_SERIALIZATION_FUNCTIONS


#define QN_DEFINE_COLLECTION_UBJSON_SERIALIZATION_FUNCTIONS(TYPE, TPL_DEF, TPL_ARG, IMPL) \
template<BOOST_PP_TUPLE_ENUM(TPL_DEF), class Output>                            \
void serialize(const TYPE<BOOST_PP_TUPLE_ENUM(TPL_ARG)> &value, QnUbjsonWriter<Output> *stream) { \
    QnUbjsonDetail::BOOST_PP_CAT(serialize_, IMPL)(value, stream);              \
}                                                                               \
                                                                                \
template<BOOST_PP_TUPLE_ENUM(TPL_DEF), class Input>                             \
bool deserialize(QnUbjsonReader<Input> *stream, TYPE<BOOST_PP_TUPLE_ENUM(TPL_ARG)> *target) { \
    return QnUbjsonDetail::BOOST_PP_CAT(deserialize_, IMPL)(stream, target);    \
}                                                                               \

QN_DEFINE_COLLECTION_UBJSON_SERIALIZATION_FUNCTIONS(QSet, (class T), (T), collection);
QN_DEFINE_COLLECTION_UBJSON_SERIALIZATION_FUNCTIONS(QList, (class T), (T), collection);
QN_DEFINE_COLLECTION_UBJSON_SERIALIZATION_FUNCTIONS(QLinkedList, (class T), (T), collection);
QN_DEFINE_COLLECTION_UBJSON_SERIALIZATION_FUNCTIONS(QVector, (class T), (T), collection);
QN_DEFINE_COLLECTION_UBJSON_SERIALIZATION_FUNCTIONS(QVarLengthArray, (class T, int N), (T, N), collection);
QN_DEFINE_COLLECTION_UBJSON_SERIALIZATION_FUNCTIONS(QMap, (class Key, class T), (Key, T), collection);
QN_DEFINE_COLLECTION_UBJSON_SERIALIZATION_FUNCTIONS(QHash, (class Key, class T), (Key, T), collection);
QN_DEFINE_COLLECTION_UBJSON_SERIALIZATION_FUNCTIONS(std::vector, (class T, class Allocator), (T, Allocator), collection);
QN_DEFINE_COLLECTION_UBJSON_SERIALIZATION_FUNCTIONS(std::set, (class Key, class Predicate, class Allocator), (Key, Predicate, Allocator), collection);
QN_DEFINE_COLLECTION_UBJSON_SERIALIZATION_FUNCTIONS(std::map, (class Key, class T, class Predicate, class Allocator), (Key, T, Predicate, Allocator), collection);

QN_DEFINE_COLLECTION_UBJSON_SERIALIZATION_FUNCTIONS(QMap, (class T), (QString, T), string_map);
QN_DEFINE_COLLECTION_UBJSON_SERIALIZATION_FUNCTIONS(QHash, (class T), (QString, T), string_map);
QN_DEFINE_COLLECTION_UBJSON_SERIALIZATION_FUNCTIONS(std::map, (class T, class Predicate, class Allocator), (QString, T, Predicate, Allocator), string_map);
#undef QN_DEFINE_COLLECTION_UBJSON_SERIALIZATION_FUNCTIONS
#endif // Q_MOC_RUN


template <class Output>
void serialize(const QUrl &value, QnUbjsonWriter<Output> *stream) {
    QnUbjson::serialize(value.toString(), stream);
}

template <class T>
bool deserialize(QnUbjsonReader<T> *stream, QUrl *target) {
    QString tmp;
    if(!QnUbjson::deserialize(stream, &tmp))
        return false;
    *target = QUrl(tmp);
    return true;
}

template <class Output>
void serialize(const nx::utils::Url& value, QnUbjsonWriter<Output> *stream)
{
    QnUbjson::serialize(value.toString(), stream);
}

template <class T>
bool deserialize(QnUbjsonReader<T> *stream, nx::utils::Url *target)
{
    QString tmp;
    if(!QnUbjson::deserialize(stream, &tmp))
        return false;
    *target = nx::utils::Url(tmp);
    return true;
}


template <class Output>
void serialize(const QnUuid &value, QnUbjsonWriter<Output> *stream) {
    QnUbjson::serialize(value.toRfc4122(), stream);
}


template <class Input>
bool deserialize(QnUbjsonReader<Input> *stream, QnUuid *target)
{
    std::array<char, 16> tmp;
    if(!stream->template readBinaryData<>(&tmp))
        return false;
    *target = QnUuid::fromRfc4122(QByteArray::fromRawData(tmp.data(), static_cast<int>(tmp.size())));
    return true;
}


template<class T, class Output>
void serialize(const T &value, QnUbjsonWriter<Output> *stream, typename std::enable_if<QnSerialization::is_enum_or_flags<T>::value>::type * = NULL) {
    QnSerialization::check_enum_binary<T>();

    QnUbjson::serialize(static_cast<qint32>(value), stream);
}

template<class T, class Input>
bool deserialize(QnUbjsonReader<Input> *stream, T *target, typename std::enable_if<QnSerialization::is_enum_or_flags<T>::value>::type * = NULL) {
    QnSerialization::check_enum_binary<T>();

    qint32 tmp;
    if(!QnUbjson::deserialize(stream, &tmp))
        return false;
    *target = static_cast<T>(tmp);
    return true;
}


template<class T, std::size_t N, class Output>
void serialize(const std::array<T, N> &value, QnUbjsonWriter<Output> *stream) {
    stream->writeArrayStart();
    for(const T &element: value)
        QnUbjson::serialize(element, stream);
    stream->writeArrayEnd();
}

template<class T, std::size_t N, class Input>
bool deserialize(QnUbjsonReader<Input> *stream, std::array<T, N> *target) {
    if(!stream->readArrayStart())
        return false;

    for(T &element: *target)
        if(!QnUbjson::deserialize(stream, &element))
            return false;

    if(!stream->readArrayEnd())
        return false;

    return true;
}

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
(QSize)(QSizeF)(QRect)(QRectF)(QPoint)(QPointF)(QVector2D)(QVector3D)(QVector4D),
(ubjson)
)

#endif // QN_UBJSON_FUNCTIONS_H
