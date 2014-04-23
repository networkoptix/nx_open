#ifndef QN_SERIALIZATION_BINARY_FUNCTIONS_H
#define QN_SERIALIZATION_BINARY_FUNCTIONS_H

#include "binary.h"

#include <QtCore/QUuid>
#include <QtCore/QString>
#include <QtCore/QUuid>

namespace QnBinaryDetail {
    
    template<class List, class Output>
    void serialize_list(const List &value, QnOutputBinaryStream<Output> *stream) {
        QnBinary::serialize(static_cast<qint32>(value.size()), stream);
        for(auto pos = value.begin(); pos != value.end(); ++pos)
            QnBinary::serialize(*pos, stream);
    }

    template <class List, class Input>
    bool deserialize_list(QnInputBinaryStream<Input> *stream, List *target) {
        qint32 size;
        if(!QnBinary::deserialize(stream, &size))
            return false;

        target->clear();
        target->reserve(size);

        for(int i = 0; i < size; i++) {
            target->push_back(typename List::value_type());
            if(!deserialize(stream, &value.back()))
                return false;
        }
        
        return true;
    }

} // namespace QnBinaryDetail


template <class Output>
void serialize(qint32 field, QnOutputBinaryStream<Output>* stream) {
    qint32 tmp = htonl(field);
    stream->write(&tmp, sizeof(tmp));
}

template <class Output>
void serialize(quint32 field, QnOutputBinaryStream<Output>* stream) {
    quint32 tmp = htonl(field);
    stream->write(&tmp, sizeof(tmp));
}

template <class Output>
void serialize(qint16 field, QnOutputBinaryStream<Output>* stream) {
    qint16 tmp = htons(field);
    stream->write(&tmp, sizeof(tmp));
}

template <class Output>
void serialize(quint16 field, QnOutputBinaryStream<Output>* stream) {
    quint16 tmp = htons(field);
    stream->write(&tmp, sizeof(tmp));
}

template <class Output>
void serialize(qint8 field, QnOutputBinaryStream<Output>* stream) {
    stream->write(&field, sizeof(field));
}

template <class Output>
void serialize(quint8 field, QnOutputBinaryStream<Output>* stream) {
    stream->write(&field, sizeof(field));
}

template <class Output>
void serialize(qint64 field, QnOutputBinaryStream<Output>* stream) {
    qint64 tmp = htonll(field);
    stream->write(&tmp, sizeof(field));
}

template <class Output>
void serialize(float field, QnOutputBinaryStream<Output>* stream) {
    stream->write(&field, sizeof(field));
}

template <class Output>
void serialize(double field, QnOutputBinaryStream<Output>* stream) {
    stream->write(&field, sizeof(field));
}

template <class Output>
void serialize(bool field, QnOutputBinaryStream<Output>* stream) {
    quint8 t = field ? 0xff : 0;
    stream->write(&t, 1);
}

template <class T>
bool deserialize(QnInputBinaryStream<T>* stream, qint32 *target) {
    qint32 tmp;
    if( stream->read(&tmp, sizeof(tmp)) != sizeof(tmp) )
        return false;
    *target = ntohl(tmp);
    return true;
}

template <class T>
bool deserialize(QnInputBinaryStream<T>* stream, quint32 *target) {
    quint32 tmp;
    if( stream->read(&tmp, sizeof(tmp)) != sizeof(tmp) )
        return false;
    *target = ntohl(tmp);
    return true;
}

template <class T>
bool deserialize(QnInputBinaryStream<T>* stream, qint16 *target) {
    qint16 tmp;
    if( stream->read(&tmp, sizeof(tmp)) != sizeof(tmp) )
        return false;
    *target = ntohs(tmp);
    return true;
}

template <class T>
bool deserialize(QnInputBinaryStream<T>* stream, quint16 *target) {
    quint16 tmp;
    if( stream->read(&tmp, sizeof(tmp)) != sizeof(tmp) )
        return false;
    *target = ntohs(tmp);
    return true;
}

template <class T>
bool deserialize(QnInputBinaryStream<T>* stream, qint8 *target) {
    if( stream->read(target, sizeof(*target)) != sizeof(*target) )
        return false;
    return true;
}

template <class T>
bool deserialize(QnInputBinaryStream<T>* stream, quint8 *target) {
    if( stream->read(target, sizeof(*target)) != sizeof(*target) )
        return false;
    return true;
}

template <class T>
bool deserialize(QnInputBinaryStream<T>* stream, qint64 *field) {
    qint64 tmp;
    if( stream->read(&tmp, sizeof(tmp)) != sizeof(tmp) )
        return false;
    *target = ntohll(tmp);
    return true;
}

template <class T>
bool deserialize(QnInputBinaryStream<T>* stream, float *target) {
    return stream->read(target, sizeof(*target)) == sizeof(*target);
}

template <class T>
bool deserialize(QnInputBinaryStream<T>* stream, double *target) {
    return stream->read(target, sizeof(*target)) == sizeof(*target);
}

template <class T>
bool deserialize(QnInputBinaryStream<T>* stream, bool *target) {
    quint8 tmp;
    if(stream->read(&tmp, 1) != 1)
        return false;
    *target = static_cast<bool>(tmp);
    return true;
}


template <class Output>
void serialize(const QByteArray& field, QnOutputBinaryStream<Output>* stream) {
    serialize((qint32) field.size(), stream);
    stream->write(field.data(), field.size());
}

template <class T>
bool deserialize(QnInputBinaryStream<T>* stream, QByteArray *target) {
    qint32 size;
    if( !deserialize(stream, &size) )
        return false;
    target->resize(size);
    return stream->read(target->data(), size) == size;
}

template <class Output>
void serialize(const QString& field, QnOutputBinaryStream<Output>* stream) {
    serialize(field.toUtf8(), stream);
}

template <class T>
bool deserialize(QnInputBinaryStream<T>* stream, QString *target) {
    QByteArray data;
    if( !deserialize(stream, &data) )
        return false;
    *target = QString::fromUtf8(data);
    return true;
}

template <class Output>
void serialize(const QUrl& field, QnOutputBinaryStream<Output>* stream) {
    serialize(field.toString(), stream);
}

template <class T>
bool deserialize(QnInputBinaryStream<T>* stream, QUrl *target) {
    QByteArray data;
    if( !deserialize(stream, &data) )
        return false;
    *target = QUrl(QString::fromUtf8(data));
    return true;
}

template <class Output>
void serialize(const QUuid &value, QnOutputBinaryStream<Output>* stream) {
    QByteArray data = field.toRfc4122();
    stream->write(data.data(), data.size());
}

template <class T>
bool deserialize(QnInputBinaryStream<T> *stream, QUuid *target) {
    QByteArray data;
    data.resize(16);
    if( stream->read(data.data(), 16) != 16 )
        return false;
    *target = QUuid::fromRfc4122(data);
    return true;
}




/*template<class T, class T2>
bool deserialize( T2& field, QnInputBinaryStream<T>* stream, typename std::enable_if<std::is_enum<T2>::value>::type* = NULL )
{
    qint32 tmp;
    if( stream->read(&tmp, sizeof(field)) != sizeof(field) )
        return false;
    field = (T2) ntohl(tmp);
    return true;
}*/


template <class T, class Output>
void serialize(const std::vector<T>& value, QnOutputBinaryStream<Output>* stream) {
    QnBinaryDetail::serialize_list(value, stream)
}

template <class T, class Input>
bool deserialize(QnInputBinaryStream<Input>* stream, std::vector<T> *target) {
    return QnBinaryDetail::deserialize_list(stream, target);
}

template <class T, class Output>
void serialize(const QList<T>& value, QnOutputBinaryStream<Output>* stream) {
    QnBinaryDetail::serialize_list(value, stream);
}

template <class T, class Input>
bool deserialize(QnInputBinaryStream<Input>* stream, QList<T> *target) {
    return QnBinaryDetail::deserialize_list(stream, target);
}

template <class Key, class T, class Predicate, class Allocator, class Output>
void serialize(const std::map<Key, T, Predicate, Allocator>& value, QnOutputBinaryStream<Output>* stream) 
{
    serialize((qint32) field.size(), stream);
    for(typename std::map<K, V>::const_iterator itr = field.begin(); itr != field.end(); ++itr) {
        serialize(itr->first, stream);
        serialize(itr->second, stream);
    }
}

template <class Key, class T, class Predicate, class Allocator, class Input>
bool deserialize(std::map<Key, T, Predicate, Allocator>& value, QnInputBinaryStream<Input>* stream) 
{
    qint32 size = 0;
    if( !deserialize(size, stream) )
        return false;
    for( qint32 i = 0; i < size; ++i )
    {
        K key;
        V value;
        if( !deserialize(key, stream) || !deserialize(value, stream))
            return false;
        field.insert(std::pair<K, V>(key, value));
    }
    return true;
}

template <class T, class T2>
void serialize(const QSet<T2>& field, QnOutputBinaryStream<T>* stream) 
{
    serialize((qint32) field.size(), stream);
    using namespace std::placeholders;
    std::for_each( field.begin(), field.end(), [stream](const T2& val){ serialize(val, stream); } );
}

template <class T, class T2, class T3>
void serialize(const QMap<T2, T3>& field, QnOutputBinaryStream<T>* stream) 
{
    serialize((qint32) field.size(), stream);
    for(typename QMap<T2, T3>::const_iterator itr = field.begin(); itr != field.end(); ++itr) {
        serialize(itr.key(), stream);
        serialize(itr.value(), stream);
    }
}

template <class T, class T2>
bool deserialize(QSet<T2>& field, QnInputBinaryStream<T>* stream) 
{
    qint32 size = 0;
    if( !deserialize(size, stream) )
        return false;
    for( qint32 i = 0; i < size; ++i )
    {
        T2 t2;
        if( !deserialize(t2, stream) )
            return false;
        field.insert( t2 );
    }
    return true;
}

template <class T, class T2, class T3>
bool deserialize(QMap<T2, T3>& field, QnInputBinaryStream<T>* stream) 
{
    qint32 size = 0;
    if( !deserialize(size, stream) )
        return false;
    for( qint32 i = 0; i < size; ++i )
    {
        T2 t2;
        T3 t3;
        if( !deserialize(t2, stream) || !deserialize(t3, stream))
            return false;
        field.insert(t2, t3);
    }
    return true;
}

#endif // QN_SERIALIZATION_BINARY_FUNCTIONS_H
