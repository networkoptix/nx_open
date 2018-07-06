#pragma once

#include <type_traits> /* For std::enable_if. */
#include <utility> /* For std::pair, std::min. */
#include <array>
#include <vector>
#include <set>
#include <map>

#ifndef Q_MOC_RUN
#include <boost/preprocessor/tuple/enum.hpp>
#endif

#include <QtCore/QtEndian>
#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QUrl>

#include <nx/utils/collection.h>
#include <nx/utils/latin1_array.h>
#include <nx/utils/uuid.h>
#include <nx/utils/url.h>

#include "collection_fwd.h"
#include "binary.h"
#include "binary_macros.h"
#include "enum.h"


namespace QnBinaryDetail {

    template<class Element, class Output, class Tag>
    void serialize_collection_element(const Element &element, QnOutputBinaryStream<Output> *stream, const Tag &) {
        QnBinary::serialize(element, stream);
    }

    template<class Element, class Output>
    void serialize_collection_element(const Element &element, QnOutputBinaryStream<Output> *stream, const QnCollection::map_tag &) {
        QnBinary::serialize(element.first, stream);
        QnBinary::serialize(element.second, stream);
    }

    template<class Collection, class Output>
    void serialize_collection(const Collection &value, QnOutputBinaryStream<Output> *stream) {
        QnBinary::serialize(static_cast<qint32>(value.size()), stream);
        for(auto pos = boost::begin(value); pos != boost::end(value); ++pos)
            serialize_collection_element(*pos, stream, typename QnCollection::collection_category<Collection>::type());
    }

    template<class Collection, class Element, class Input>
    bool deserialize_collection_element(QnInputBinaryStream<Input> *stream, Collection *target, const Element *, const QnCollection::list_tag &) {
        /* Deserialize value right into list. */
        return QnBinary::deserialize(stream, &*QnCollection::insert(*target, boost::end(*target), Element()));
    }

    template<class Collection, class Element, class Input>
    bool deserialize_collection_element(QnInputBinaryStream<Input> *stream, Collection *target, const Element *, const QnCollection::set_tag &) {
        Element element;
        if(!QnBinary::deserialize(stream, &element))
            return false;

        QnCollection::insert(*target, boost::end(*target), std::move(element));
        return true;
    }

    template<class Collection, class Element, class Input>
    bool deserialize_collection_element(QnInputBinaryStream<Input> *stream, Collection *target, const Element *, const QnCollection::map_tag &) {
        typename Collection::key_type key;
        if(!QnBinary::deserialize(stream, &key))
            return false;

        /* Deserialize mapped value right into container. */
        if(!QnBinary::deserialize(stream, &(*target)[key]))
            return false;

        return true;
    }

    template<class Collection, class Input>
    bool deserialize_collection(QnInputBinaryStream<Input> *stream, Collection *target) {
        typedef typename QnCollection::make_assignable<typename std::iterator_traits<typename boost::range_mutable_iterator<Collection>::type>::value_type>::type value_type;

        qint32 size;
        if(!QnBinary::deserialize(stream, &size))
            return false;
        if(size < 0)
            return false;

        /* Don't reserve too much. Big size may be a result of invalid input,
         * and in this case we don't want to crash with out-of-memory exception.
         * Limit: no more than 1024 elements, and no more than 1Mb of memory. */
        qint32 maxSize = std::min(1024, 1024 * 1024 / static_cast<int>(sizeof(value_type)));

        QnCollection::clear(*target);
        QnCollection::reserve(*target, std::min(size, maxSize));

        for(int i = 0; i < size; i++)
            if(!deserialize_collection_element(stream, target, static_cast<const value_type *>(NULL), typename QnCollection::collection_category<Collection>::type()))
                return false;

        return true;
    }

} // namespace QnBinaryDetail


template<class Output>
void serialize(qint32 value, QnOutputBinaryStream<Output> *stream) {
    qint32 tmp = qToBigEndian(value);
    stream->write(&tmp, sizeof(tmp));
}

template<class Output>
void serialize(quint32 value, QnOutputBinaryStream<Output> *stream) {
    quint32 tmp = qToBigEndian(value);
    stream->write(&tmp, sizeof(tmp));
}

template<class Output>
void serialize(qint16 value, QnOutputBinaryStream<Output> *stream) {
    qint16 tmp = qToBigEndian(value);
    stream->write(&tmp, sizeof(tmp));
}

template<class Output>
void serialize(quint16 value, QnOutputBinaryStream<Output> *stream) {
    quint16 tmp = qToBigEndian(value);
    stream->write(&tmp, sizeof(tmp));
}

template<class Output>
void serialize(qint8 value, QnOutputBinaryStream<Output> *stream) {
    stream->write(&value, sizeof(value));
}

template<class Output>
void serialize(quint8 value, QnOutputBinaryStream<Output> *stream) {
    stream->write(&value, sizeof(value));
}

template<class Output>
void serialize(qint64 value, QnOutputBinaryStream<Output> *stream) {
    qint64 tmp = qToBigEndian(value);
    stream->write(&tmp, sizeof(value));
}

template<class Output>
void serialize(quint64 value, QnOutputBinaryStream<Output> *stream) {
    quint64 tmp = qToBigEndian(value);
    stream->write(&tmp, sizeof(value));
}

template<class Output>
void serialize(float value, QnOutputBinaryStream<Output> *stream) {
    stream->write(&value, sizeof(value));
}

template<class Output>
void serialize(double value, QnOutputBinaryStream<Output> *stream) {
    stream->write(&value, sizeof(value));
}

template<class Output>
void serialize(bool value, QnOutputBinaryStream<Output> *stream) {
    quint8 t = value ? 0xff : 0;
    stream->write(&t, 1);
}

template<class Input>
bool deserialize(QnInputBinaryStream<Input> *stream, qint32 *target) {
    qint32 tmp;
    if(stream->read(&tmp, sizeof(tmp)) != sizeof(tmp))
        return false;
    *target = qFromBigEndian(tmp);
    return true;
}

template<class Input>
bool deserialize(QnInputBinaryStream<Input> *stream, quint32 *target) {
    quint32 tmp;
    if(stream->read(&tmp, sizeof(tmp)) != sizeof(tmp))
        return false;
    *target = qFromBigEndian(tmp);
    return true;
}

template<class Input>
bool deserialize(QnInputBinaryStream<Input> *stream, qint16 *target) {
    qint16 tmp;
    if(stream->read(&tmp, sizeof(tmp)) != sizeof(tmp))
        return false;
    *target = qFromBigEndian(tmp);
    return true;
}

template<class Input>
bool deserialize(QnInputBinaryStream<Input> *stream, quint16 *target) {
    quint16 tmp;
    if(stream->read(&tmp, sizeof(tmp)) != sizeof(tmp))
        return false;
    *target = qFromBigEndian(tmp);
    return true;
}

template<class Input>
bool deserialize(QnInputBinaryStream<Input> *stream, qint8 *target) {
    if(stream->read(target, sizeof(*target)) != sizeof(*target))
        return false;
    return true;
}

template<class Input>
bool deserialize(QnInputBinaryStream<Input> *stream, quint8 *target) {
    if(stream->read(target, sizeof(*target)) != sizeof(*target))
        return false;
    return true;
}

template<class Input>
bool deserialize(QnInputBinaryStream<Input> *stream, qint64 *target) {
    qint64 tmp;
    if(stream->read(&tmp, sizeof(tmp)) != sizeof(tmp) )
        return false;
    *target = qFromBigEndian(tmp);
    return true;
}

template<class Input>
bool deserialize(QnInputBinaryStream<Input> *stream, quint64 *target) {
    quint64 tmp;
    if(stream->read(&tmp, sizeof(tmp)) != sizeof(tmp) )
        return false;
    *target = qFromBigEndian(tmp);
    return true;
}

template<class Input>
bool deserialize(QnInputBinaryStream<Input> *stream, float *target) {
    return stream->read(target, sizeof(*target)) == sizeof(*target);
}

template<class Input>
bool deserialize(QnInputBinaryStream<Input> *stream, double *target) {
    return stream->read(target, sizeof(*target)) == sizeof(*target);
}

template<class Input>
bool deserialize(QnInputBinaryStream<Input> *stream, bool *target) {
    quint8 tmp;
    if(stream->read(&tmp, 1) != 1)
        return false;
    *target = static_cast<bool>(tmp);
    return true;
}

template<class Output>
void serialize(const QByteArray &value, QnOutputBinaryStream<Output> *stream) {
    serialize(static_cast<qint32>(value.size()), stream);
    stream->write(value.data(), value.size());
}

template<class T>
bool deserialize(QnInputBinaryStream<T> *stream, QByteArray *target) {
    qint32 size = 0;
    if(!QnBinary::deserialize(stream, &size))
        return false;
    if(size < 0)
        return false;

    const qint32 chunkSize = 16 * 1024 * 1024;
    if(size < chunkSize) {
        /* If it's less than 16Mb then just read it as a whole chunk. */
        target->resize(size);
        return stream->read(target->data(), size) == size;
    } else {
        /* Otherwise there is a high probability that the stream is corrupted,
         * but we cannot be 100% sure. Read it chunk-by-chunk, then assemble. */
        QVector<QByteArray> chunks;

        for(qint32 pos = 0; pos < size; pos += chunkSize) {
            QByteArray chunk;
            chunk.resize(std::min(chunkSize, size - pos));

            if(stream->read(chunk.data(), chunk.size()) != chunk.size())
                return false; /* The stream was indeed corrupted. */

            chunks.push_back(chunk);
        }

        /* Assemble the chunks. */
        target->clear();
        target->reserve(size);
        for(const QByteArray chunk: chunks)
            target->append(chunk);
        return true;
    }
}


template<class Output>
void serialize(const QnLatin1Array &value, QnOutputBinaryStream<Output> *stream) {
    serialize(static_cast<const QByteArray &>(value), stream);
}

template<class T>
bool deserialize(QnInputBinaryStream<T> *stream, QnLatin1Array *target) {
    return deserialize(stream, static_cast<QByteArray *>(target));
}


template<class Output>
void serialize(const QString &value, QnOutputBinaryStream<Output> *stream) {
    serialize(value.toUtf8(), stream);
}

template <class T>
bool deserialize(QnInputBinaryStream<T> *stream, QString *target) {
    QByteArray data;
    if(!QnBinary::deserialize(stream, &data))
        return false;
    *target = QString::fromUtf8(data);
    return true;
}


template <class Output>
void serialize(const QUrl &value, QnOutputBinaryStream<Output> *stream) {
    QnBinary::serialize(value.toString(), stream);
}

template <class T>
bool deserialize(QnInputBinaryStream<T> *stream, QUrl *target) {
    QString tmp;
    if(!QnBinary::deserialize(stream, &tmp))
        return false;
    *target = QUrl(tmp);
    return true;
}

template <class Output>
void serialize(const nx::utils::Url &value, QnOutputBinaryStream<Output> *stream) {
    QnBinary::serialize(value.toString(), stream);
}

template <class T>
bool deserialize(QnInputBinaryStream<T> *stream, nx::utils::Url *target) {
    QString tmp;
    if(!QnBinary::deserialize(stream, &tmp))
        return false;
    *target = nx::utils::Url(tmp);
    return true;
}


template <class Output>
void serialize(const QnUuid &value, QnOutputBinaryStream<Output> *stream) {
    QByteArray tmp = value.toRfc4122();
    stream->write(tmp.data(), tmp.size());
}

template <class Input>
bool deserialize(QnInputBinaryStream<Input> *stream, QnUuid *target) {
    QByteArray tmp;
    tmp.resize(16);
    if( stream->read(tmp.data(), 16) != 16 )
        return false;
    *target = QnUuid::fromRfc4122(tmp);
    return true;
}

// TODO: #Elric #ec2 check all enums we're using for explicit values.

template<class T, class Output>
void serialize(const T &value, QnOutputBinaryStream<Output> *stream, typename std::enable_if<QnSerialization::is_enum_or_flags<T>::value>::type * = NULL) {
    QnSerialization::check_enum_binary<T>();

    QnBinary::serialize(static_cast<qint32>(value), stream);
}

template<class T, class Input>
bool deserialize(QnInputBinaryStream<Input> *stream, T *target, typename std::enable_if<QnSerialization::is_enum_or_flags<T>::value>::type * = NULL) {
    QnSerialization::check_enum_binary<T>();

    qint32 tmp;
    if(!QnBinary::deserialize(stream, &tmp))
        return false;
    *target = static_cast<T>(tmp);
    return true;
}


#ifndef Q_MOC_RUN
#define QN_DEFINE_COLLECTION_BINARY_SERIALIZATION_FUNCTIONS(TYPE, TPL_DEF, TPL_ARG) \
template<BOOST_PP_TUPLE_ENUM(TPL_DEF), class Output>                            \
void serialize(const TYPE<BOOST_PP_TUPLE_ENUM(TPL_ARG)> &value, QnOutputBinaryStream<Output> *stream) { \
    QnBinaryDetail::serialize_collection(value, stream);                         \
}                                                                               \
                                                                                \
template<BOOST_PP_TUPLE_ENUM(TPL_DEF), class Input>                             \
bool deserialize(QnInputBinaryStream<Input> *stream, TYPE<BOOST_PP_TUPLE_ENUM(TPL_ARG)> *target) { \
    return QnBinaryDetail::deserialize_collection(stream, target);               \
}                                                                               \

QN_DEFINE_COLLECTION_BINARY_SERIALIZATION_FUNCTIONS(QSet, (class T), (T));
QN_DEFINE_COLLECTION_BINARY_SERIALIZATION_FUNCTIONS(QList, (class T), (T));
QN_DEFINE_COLLECTION_BINARY_SERIALIZATION_FUNCTIONS(QLinkedList, (class T), (T));
QN_DEFINE_COLLECTION_BINARY_SERIALIZATION_FUNCTIONS(QVector, (class T), (T));
QN_DEFINE_COLLECTION_BINARY_SERIALIZATION_FUNCTIONS(QVarLengthArray, (class T, int N), (T, N));
QN_DEFINE_COLLECTION_BINARY_SERIALIZATION_FUNCTIONS(QMap, (class Key, class T), (Key, T));
QN_DEFINE_COLLECTION_BINARY_SERIALIZATION_FUNCTIONS(QHash, (class Key, class T), (Key, T));
QN_DEFINE_COLLECTION_BINARY_SERIALIZATION_FUNCTIONS(std::vector, (class T, class Allocator), (T, Allocator));
QN_DEFINE_COLLECTION_BINARY_SERIALIZATION_FUNCTIONS(std::set, (class Key, class Predicate, class Allocator), (Key, Predicate, Allocator));
QN_DEFINE_COLLECTION_BINARY_SERIALIZATION_FUNCTIONS(std::map, (class Key, class T, class Predicate, class Allocator), (Key, T, Predicate, Allocator));
#undef QN_DEFINE_COLLECTION_BINARY_SERIALIZATION_FUNCTIONS
#endif // Q_MOC_RUN


template<class T1, class T2, class Output>
void serialize(const std::pair<T1, T2> &value, QnOutputBinaryStream<Output> *stream) {
    QnBinary::serialize(value.first, stream);
    QnBinary::serialize(value.second, stream);
}

template<class T1, class T2, class Input>
bool deserialize(QnInputBinaryStream<Input> *stream, std::pair<T1, T2> *target) {
    return
        QnBinary::deserialize(stream, &target->first) &&
        QnBinary::deserialize(stream, &target->second);
}

template<class T, std::size_t N, class Output>
void serialize(const std::array<T, N> &value, QnOutputBinaryStream<Output> *stream) {
    for(const T &element: value)
        QnBinary::serialize(element, stream);
}

template<class T, std::size_t N, class Input>
bool deserialize(QnInputBinaryStream<Input> *stream, std::array<T, N> *target) {
    for(T &element: *target)
        if(!QnBinary::deserialize(stream, &element))
            return false;
    return true;
}
