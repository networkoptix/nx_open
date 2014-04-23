#ifndef QN_SERIALIZATION_BINARY_FUNCTIONS_H
#define QN_SERIALIZATION_BINARY_FUNCTIONS_H

#include "binary.h"

#include <utility> /* For std::pair. */

#include <QtCore/QUuid>
#include <QtCore/QString>
#include <QtCore/QUuid>

#include <utils/common/container.h>

namespace QnBinaryDetail {
    
    template<class Container, class Output>
    void serialize_container(const Container &value, QnOutputBinaryStream<Output> *stream) {
        QnBinary::serialize(static_cast<qint32>(value.size()), stream);
        for(auto pos = boost::begin(value); pos != boost::end(value); ++pos)
            QnBinary::serialize(*pos, stream);
    }

    template <class Container, class Input>
    bool deserialize_container(QnInputBinaryStream<Input> *stream, Container *target) {
        qint32 size;
        if(!QnBinary::deserialize(stream, &size))
            return false;

        QnContainer::clear(*target);
        QnContainer::reserve(*target, size);

        for(int i = 0; i < size; i++) {
            typename boost::range_mutable_iterator<Container>::type::value_type element;
            if(!QnBinary::deserialize(stream, &element))
                return false;
            QnContainer::insert(*target, boost::end(*target), element);
        }
        
        return true;
    }

} // namespace QnBinaryDetail


template <class Output>
void serialize(qint32 value, QnOutputBinaryStream<Output> *stream) {
    qint32 tmp = htonl(value);
    stream->write(&tmp, sizeof(tmp));
}

template <class Output>
void serialize(quint32 value, QnOutputBinaryStream<Output> *stream) {
    quint32 tmp = htonl(value);
    stream->write(&tmp, sizeof(tmp));
}

template <class Output>
void serialize(qint16 value, QnOutputBinaryStream<Output> *stream) {
    qint16 tmp = htons(value);
    stream->write(&tmp, sizeof(tmp));
}

template <class Output>
void serialize(quint16 value, QnOutputBinaryStream<Output> *stream) {
    quint16 tmp = htons(value);
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
    qint64 tmp = htonll(value);
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
    *target = ntohl(tmp);
    return true;
}

template <class T>
bool deserialize(QnInputBinaryStream<T> *stream, quint32 *target) {
    quint32 tmp;
    if( stream->read(&tmp, sizeof(tmp)) != sizeof(tmp) )
        return false;
    *target = ntohl(tmp);
    return true;
}

template <class T>
bool deserialize(QnInputBinaryStream<T> *stream, qint16 *target) {
    qint16 tmp;
    if( stream->read(&tmp, sizeof(tmp)) != sizeof(tmp) )
        return false;
    *target = ntohs(tmp);
    return true;
}

template <class T>
bool deserialize(QnInputBinaryStream<T> *stream, quint16 *target) {
    quint16 tmp;
    if( stream->read(&tmp, sizeof(tmp)) != sizeof(tmp) )
        return false;
    *target = ntohs(tmp);
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
    *target = ntohll(tmp);
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
    serialize((qint32) value.size(), stream);
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
    QByteArray data;
    if(!QnBinary::deserialize(stream, &data))
        return false;
    *target = QUrl(QString::fromUtf8(data));
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


template<class T1, class T2, class Output>
void serialize(const std::pair<T1, T2> &value, QnOutputBinaryStream<Output> *stream) {
    QnBinary::serialize(value.first);
    QnBinary::serialize(value.second);
}

template<class T1, class T2, class Input>
void serialize(QnInputBinaryStream<Input> *stream, const std::pair<T1, T2> *target) {
    return 
        QnBinary::deserialize(stream, &target->first) &&
        QnBinary::deserialize(stream, &target->second);
}




/*template<class T, class T2>
bool deserialize( T2& value, QnInputBinaryStream<T> *stream, typename std::enable_if<std::is_enum<T2>::value>::type* = NULL )
{
    qint32 tmp;
    if( stream->read(&tmp, sizeof(value)) != sizeof(value) )
        return false;
    value = (T2) ntohl(tmp);
    return true;
}*/



template <class T, class Output>
void serialize(const std::vector<T> &value, QnOutputBinaryStream<Output> *stream) {
    QnBinaryDetail::serialize_container(value, stream)
}

template <class T, class Input>
bool deserialize(QnInputBinaryStream<Input> *stream, std::vector<T> *target) {
    return QnBinaryDetail::deserialize_container(stream, target);
}

template <class T, class Output>
void serialize(const QList<T> &value, QnOutputBinaryStream<Output> *stream) {
    QnBinaryDetail::serialize_container(value, stream);
}

template <class T, class Input>
bool deserialize(QnInputBinaryStream<Input> *stream, QList<T> *target) {
    return QnBinaryDetail::deserialize_container(stream, target);
}

template <class Key, class T, class Predicate, class Allocator, class Output>
void serialize(const std::map<Key, T, Predicate, Allocator> &value, QnOutputBinaryStream<Output> *stream) {
    QnBinaryDetail::serialize_container(value, stream);
}

template <class Key, class T, class Predicate, class Allocator, class Input>
bool deserialize(QnInputBinaryStream<Input> *stream, std::map<Key, T, Predicate, Allocator> *target) {
    return QnBinaryDetail::deserialize_container(stream, target);
}

template <class T, class Output>
void serialize(const QSet<T> &value, QnOutputBinaryStream<Output> *stream) {
    return QnBinaryDetail::serialize_container(value, stream);
}

template <class T, class Input>
bool deserialize(QnInputBinaryStream<Input> *stream, QSet<T> *target) {
    return QnBinaryDetail::deserialize_container(stream, target);
}

template <class Key, class T, class Output>
void serialize(const QMap<Key, T> &value, QnOutputBinaryStream<Output> *stream) {
    return QnBinaryDetail::serialize_container(value, stream);
}

template <class Key, class T, class Input>
bool deserialize(QnInputBinaryStream<Input> *stream, QMap<Key, T> *target) {
    return QnBinaryDetail::deserialize_container(stream, target);
}

#endif // QN_SERIALIZATION_BINARY_FUNCTIONS_H
