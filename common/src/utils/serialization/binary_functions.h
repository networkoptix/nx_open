#ifndef QN_SERIALIZATION_BINARY_FUNCTIONS_H
#define QN_SERIALIZATION_BINARY_FUNCTIONS_H

#include "binary.h"

#include <utility> /* For std::pair. */
#include <array>
#include <vector>
#include <set>
#include <map>

#include <boost/preprocessor/tuple/enum.hpp>

#include <QtCore/QtEndian>
#include <QtCore/QString>
#include <QtCore/QUuid>
#include <QtCore/QList>
#include <QtCore/QLinkedList>
#include <QtCore/QVector>
#include <QtCore/QMap>
#include <QtCore/QSet>
#include <QtCore/QHash>
#include <QtCore/QVarLengthArray>

#include <utils/common/container.h>

namespace QnBinaryDetail {
    // TODO: #Elric #EC2 remove / move
    template<class T>
    struct identity {
        typedef T type;
    };

    template<class Element, class Output, class Tag>
    void serialize_container_element(const Element &element, QnOutputBinaryStream<Output> *stream, const Tag &) {
        QnBinary::serialize(element, stream);
    }

    template<class Element, class Output>
    void serialize_container_element(const Element &element, QnOutputBinaryStream<Output> *stream, const QnContainer::map_tag &) {
        QnBinary::serialize(element.first, stream);
        QnBinary::serialize(element.second, stream);
    }

    template<class Container, class Output>
    void serialize_container(const Container &value, QnOutputBinaryStream<Output> *stream) {
        QnBinary::serialize(static_cast<qint32>(value.size()), stream);
        for(auto pos = boost::begin(value); pos != boost::end(value); ++pos)
            serialize_container_element(*pos, stream, typename QnContainer::container_category<Container>::type());
    }

    template<class Container, class Element, class Input>
    bool deserialize_container_element(QnInputBinaryStream<Input> *stream, Container *target, const identity<Element> &, const QnContainer::list_tag &) {
        /* Deserialize value right into container. */
        return QnBinary::deserialize(stream, &*QnContainer::insert(*target, boost::end(*target), Element()));
    }

    template<class Container, class Element, class Input>
    bool deserialize_container_element(QnInputBinaryStream<Input> *stream, Container *target, const identity<Element> &, const QnContainer::set_tag &) {
        Element element;
        if(!QnBinary::deserialize(stream, &element))
            return false;
        
        QnContainer::insert(*target, boost::end(*target), std::move(element));
        return true;
    }

    template<class Container, class Element, class Input>
    bool deserialize_container_element(QnInputBinaryStream<Input> *stream, Container *target, const identity<Element> &, const QnContainer::map_tag &) {
        typename Container::key_type key;
        if(!QnBinary::deserialize(stream, &key))
            return false;

        /* Deserialize mapped value right into container. */
        if(!QnBinary::deserialize(stream, &(*target)[key]))
            return false;

        return true;
    }

    template<class Container, class Input>
    bool deserialize_container(QnInputBinaryStream<Input> *stream, Container *target) {
        typedef typename QnContainer::make_assignable<typename std::iterator_traits<typename boost::range_mutable_iterator<Container>::type>::value_type>::type value_type;

        qint32 size;
        if(!QnBinary::deserialize(stream, &size))
            return false;

        QnContainer::clear(*target);
        QnContainer::reserve(*target, size);

        for(int i = 0; i < size; i++)
            if(!deserialize_container_element(stream, target, identity<value_type>(), typename QnContainer::container_category<Container>::type()))
                return false;
        
        return true;
    }

} // namespace QnBinaryDetail


template <class Output>
void serialize(qint32 value, QnOutputBinaryStream<Output> *stream) {
    qint32 tmp = qToBigEndian(value);
    stream->write(&tmp, sizeof(tmp));
}

template <class Output>
void serialize(quint32 value, QnOutputBinaryStream<Output> *stream) {
    quint32 tmp = qToBigEndian(value);
    stream->write(&tmp, sizeof(tmp));
}

template <class Output>
void serialize(qint16 value, QnOutputBinaryStream<Output> *stream) {
    qint16 tmp = qToBigEndian(value);
    stream->write(&tmp, sizeof(tmp));
}

template <class Output>
void serialize(quint16 value, QnOutputBinaryStream<Output> *stream) {
    quint16 tmp = qToBigEndian(value);
    stream->write(&tmp, sizeof(tmp));
}

template <class Output>
void serialize(qint8 value, QnOutputBinaryStream<Output> *stream) {
    stream->write(&value, sizeof(value));
}

template <class Output>
void serialize(quint8 value, QnOutputBinaryStream<Output> *stream) {
    stream->write(&value, sizeof(value));
}

template <class Output>
void serialize(qint64 value, QnOutputBinaryStream<Output> *stream) {
    qint64 tmp = qToBigEndian(value);
    stream->write(&tmp, sizeof(value));
}

template <class Output>
void serialize(float value, QnOutputBinaryStream<Output> *stream) {
    stream->write(&value, sizeof(value));
}

template <class Output>
void serialize(double value, QnOutputBinaryStream<Output> *stream) {
    stream->write(&value, sizeof(value));
}

template <class Output>
void serialize(bool value, QnOutputBinaryStream<Output> *stream) {
    quint8 t = value ? 0xff : 0;
    stream->write(&t, 1);
}

template <class T>
bool deserialize(QnInputBinaryStream<T> *stream, qint32 *target) {
    qint32 tmp;
    if( stream->read(&tmp, sizeof(tmp)) != sizeof(tmp) )
        return false;
    *target = qFromBigEndian(tmp);
    return true;
}

template <class T>
bool deserialize(QnInputBinaryStream<T> *stream, quint32 *target) {
    quint32 tmp;
    if( stream->read(&tmp, sizeof(tmp)) != sizeof(tmp) )
        return false;
    *target = qFromBigEndian(tmp);
    return true;
}

template <class T>
bool deserialize(QnInputBinaryStream<T> *stream, qint16 *target) {
    qint16 tmp;
    if( stream->read(&tmp, sizeof(tmp)) != sizeof(tmp) )
        return false;
    *target = qFromBigEndian(tmp);
    return true;
}

template <class T>
bool deserialize(QnInputBinaryStream<T> *stream, quint16 *target) {
    quint16 tmp;
    if( stream->read(&tmp, sizeof(tmp)) != sizeof(tmp) )
        return false;
    *target = qFromBigEndian(tmp);
    return true;
}

template <class T>
bool deserialize(QnInputBinaryStream<T> *stream, qint8 *target) {
    if( stream->read(target, sizeof(*target)) != sizeof(*target) )
        return false;
    return true;
}

template <class T>
bool deserialize(QnInputBinaryStream<T> *stream, quint8 *target) {
    if( stream->read(target, sizeof(*target)) != sizeof(*target) )
        return false;
    return true;
}

template <class T>
bool deserialize(QnInputBinaryStream<T> *stream, qint64 *target) {
    qint64 tmp;
    if( stream->read(&tmp, sizeof(tmp)) != sizeof(tmp) )
        return false;
    *target = qFromBigEndian(tmp);
    return true;
}

template <class T>
bool deserialize(QnInputBinaryStream<T> *stream, float *target) {
    return stream->read(target, sizeof(*target)) == sizeof(*target);
}

template <class T>
bool deserialize(QnInputBinaryStream<T> *stream, double *target) {
    return stream->read(target, sizeof(*target)) == sizeof(*target);
}

template <class T>
bool deserialize(QnInputBinaryStream<T> *stream, bool *target) {
    quint8 tmp;
    if(stream->read(&tmp, 1) != 1)
        return false;
    *target = static_cast<bool>(tmp);
    return true;
}


template <class Output>
void serialize(const QByteArray &value, QnOutputBinaryStream<Output> *stream) {
    serialize(static_cast<qint32>(value.size()), stream);
    stream->write(value.data(), value.size());
}

template <class T>
bool deserialize(QnInputBinaryStream<T> *stream, QByteArray *target) {
    qint32 size;
    if(!QnBinary::deserialize(stream, &size))
        return false;
    target->resize(size);
    return stream->read(target->data(), size) == size;
}


template <class Output>
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
    QString data;
    if(!QnBinary::deserialize(stream, &data))
        return false;
    *target = QUrl(data);
    return true;
}


template <class Output>
void serialize(const QUuid &value, QnOutputBinaryStream<Output> *stream) {
    QByteArray data = value.toRfc4122();
    stream->write(data.data(), data.size());
}

template <class Input>
bool deserialize(QnInputBinaryStream<Input> *stream, QUuid *target) {
    QByteArray data;
    data.resize(16);
    if( stream->read(data.data(), 16) != 16 )
        return false;
    *target = QUuid::fromRfc4122(data);
    return true;
}


// TODO: #Elric #ec2 check all enums we're using for explicit values.

template<class T, class Output>
void serialize(const T &value, QnOutputBinaryStream<Output> *stream, typename std::enable_if<std::is_enum<T>::value>::type * = NULL) {
    QnBinary::serialize(static_cast<qint32>(value), stream);
}

template<class T, class Input>
bool deserialize(QnInputBinaryStream<Input> *stream, T *target, typename std::enable_if<std::is_enum<T>::value>::type* = NULL) {
    qint32 tmp;
    if(!QnBinary::deserialize(stream, &tmp))
        return false;
    *target = static_cast<T>(tmp);
    return true;
}


template<class T, class Output>
void serialize(const QFlags<T> &value, QnOutputBinaryStream<Output> *stream) {
    QnBinary::serialize(static_cast<qint32>(value), stream);
}

template<class T, class Input>
bool deserialize(QnInputBinaryStream<Input> *stream, QFlags<T> *target) {
    qint32 tmp;
    if(!QnBinary::deserialize(stream, &tmp))
        return false;
    *target = static_cast<QFlags<T> >(tmp);
    return true;
}


#ifndef Q_MOC_RUN
#define QN_DEFINE_CONTAINER_BINARY_SERIALIZATION_FUNCTIONS(TYPE, TPL_DEF, TPL_ARG) \
template<BOOST_PP_TUPLE_ENUM(TPL_DEF), class Output>                            \
void serialize(const TYPE<BOOST_PP_TUPLE_ENUM(TPL_ARG)> &value, QnOutputBinaryStream<Output> *stream) { \
    QnBinaryDetail::serialize_container(value, stream);                         \
}                                                                               \
                                                                                \
template<BOOST_PP_TUPLE_ENUM(TPL_DEF), class Input>                             \
bool deserialize(QnInputBinaryStream<Input> *stream, TYPE<BOOST_PP_TUPLE_ENUM(TPL_ARG)> *target) { \
    return QnBinaryDetail::deserialize_container(stream, target);               \
}                                                                               \

QN_DEFINE_CONTAINER_BINARY_SERIALIZATION_FUNCTIONS(QSet, (class T), (T));
QN_DEFINE_CONTAINER_BINARY_SERIALIZATION_FUNCTIONS(QList, (class T), (T));
QN_DEFINE_CONTAINER_BINARY_SERIALIZATION_FUNCTIONS(QLinkedList, (class T), (T));
QN_DEFINE_CONTAINER_BINARY_SERIALIZATION_FUNCTIONS(QVector, (class T), (T));
QN_DEFINE_CONTAINER_BINARY_SERIALIZATION_FUNCTIONS(QVarLengthArray, (class T, int N), (T, N));
QN_DEFINE_CONTAINER_BINARY_SERIALIZATION_FUNCTIONS(QMap, (class Key, class T), (Key, T));
QN_DEFINE_CONTAINER_BINARY_SERIALIZATION_FUNCTIONS(QHash, (class Key, class T), (Key, T));
QN_DEFINE_CONTAINER_BINARY_SERIALIZATION_FUNCTIONS(std::vector, (class T, class Allocator), (T, Allocator));
QN_DEFINE_CONTAINER_BINARY_SERIALIZATION_FUNCTIONS(std::set, (class Key, class Predicate, class Allocator), (Key, Predicate, Allocator));
QN_DEFINE_CONTAINER_BINARY_SERIALIZATION_FUNCTIONS(std::map, (class Key, class T, class Predicate, class Allocator), (Key, T, Predicate, Allocator));
#undef QN_DEFINE_CONTAINER_BINARY_SERIALIZATION_FUNCTIONS
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


#endif // QN_SERIALIZATION_BINARY_FUNCTIONS_H
