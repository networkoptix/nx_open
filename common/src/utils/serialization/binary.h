#ifndef QN_SERIALIZATION_BINARY_H
#define QN_SERIALIZATION_BINARY_H

#include <QtCore/QByteArray>

#include <utils/serialization/serialization.h>

#include "binary_fwd.h"

template<>
class QnInputBinaryStream<QByteArray> {
public:
    QnInputBinaryStream( const QByteArray& data )
    :
        m_data( data ),
        pos( 0 )
    {
    }

    /*!
        \return Bytes actually read
    */
    int read(void *buffer, int maxSize) {
        int toRead = qMin(m_data.size() - pos, maxSize);
        memcpy(buffer, m_data.constData() + pos, toRead);
        pos += toRead;
        return toRead;
    }

    //!Resets internal cursor position
    void reset() {
        pos = 0;
    }

    const QByteArray& buffer() const { return m_data; }
    int getPos() const { return pos; }

private:
    const QByteArray& m_data;
    int pos;
};

template<>
class QnOutputBinaryStream<QByteArray> {
public:
    QnOutputBinaryStream( QByteArray* const data )
    :
        m_data( data )
    {
    }

    int write(const void *data, int size) {
        m_data->append(static_cast<const char *>(data), size);
        return size;
    }

private:
    QByteArray* const m_data;
};


namespace QnBinary {
    template<class T, class Output>
    void serialize(const T &value, QnOutputBinaryStream<Output> *stream) {
        QnSerialization::serialize(value, stream);
    }

    template<class T, class Input>
    bool deserialize(QnInputBinaryStream<Input> *stream, T *target) {
        return QnSerialization::deserialize(stream, target);
    }
} // namespace QnBinary


namespace QnBinaryDetail {
    template<class Output>
    class SerializationVisitor {
    public:
        SerializationVisitor(QnOutputBinaryStream<Output> *stream): 
            m_stream(stream) 
        {}

        template<class T, class Access>
        bool operator()(const T &value, const Access &access) {
            using namespace QnFusion;

            QnBinary::serialize(invoke(access(getter), value), m_stream);
            return true;
        }

    private:
        QnOutputBinaryStream<Output> *m_stream;
    };

    template<class Input>
    struct DeserializationVisitor {
    public:
        DeserializationVisitor(QnInputBinaryStream<Input> *stream):
            m_stream(stream)
        {}

        template<class T, class Access>
        bool operator()(T &target, const Access &access) {
            using namespace QnFusion;

            return operator()(target, access, access(setter_tag));
        }

    private:
        template<class T, class Access>
        bool operator()(T &target, const Access &access, const QnFusion::member_setter_tag &) {
            using namespace QnFusion;

            return QnBinary::deserialize(m_stream, &(target.*access(setter)));
        }

        template<class T, class Access, class Member>
        bool operator()(T &target, const Access &access, const QnFusion::typed_function_setter_tag<Member> &) {
            using namespace QnFusion;

            Member member;
            if(!QnBinary::deserialize(m_stream, &member))
                return false;
            invoke(access(setter), target, std::move(member));
            return true;
        }

    private:
        QnInputBinaryStream<Input> *m_stream;
    };

} // namespace QJsonDetail


//QN_FUSION_REGISTER_SERIALIZATION_VISITORS(QJsonValue, QJsonDetail::SerializationVisitor, QJsonDetail::DeserializationVisitor)

#if 0
//namespace ec2 {

    template <class T>
    void serialize(const QUuid& field, QnOutputBinaryStream<T>* binStream) {
        QByteArray data = field.toRfc4122();
        binStream->write(data.data(), data.size());
    }

    template <class T>
    void serialize(qint32 field, QnOutputBinaryStream<T>* binStream) {
        qint32 tmp = htonl(field);
        binStream->write(&tmp, sizeof(tmp));
    }

    template <class T>
    void serialize(quint32 field, QnOutputBinaryStream<T>* binStream) {
        quint32 tmp = htonl(field);
        binStream->write(&tmp, sizeof(tmp));
    }

    template <class T>
    void serialize(qint16 field, QnOutputBinaryStream<T>* binStream) {
        qint16 tmp = htons(field);
        binStream->write(&tmp, sizeof(tmp));
    }

    template <class T>
    void serialize(quint16 field, QnOutputBinaryStream<T>* binStream) {
        quint16 tmp = htons(field);
        binStream->write(&tmp, sizeof(tmp));
    }

    template <class T>
    void serialize(qint8 field, QnOutputBinaryStream<T>* binStream) {
        binStream->write(&field, sizeof(field));
    }

    template <class T>
    void serialize(quint8 field, QnOutputBinaryStream<T>* binStream) {
        binStream->write(&field, sizeof(field));
    }

    template <class T>
    void serialize(qint64 field, QnOutputBinaryStream<T>* binStream) {
        qint64 tmp = htonll(field);
        binStream->write(&tmp, sizeof(field));
    }

    template <class T>
    void serialize(float field, QnOutputBinaryStream<T>* binStream) {
        binStream->write(&field, sizeof(field));
    }

    template <class T>
    void serialize(double field, QnOutputBinaryStream<T>* binStream) {
        binStream->write(&field, sizeof(field));
    }

    template <class T>
    void serialize(bool field, QnOutputBinaryStream<T>* binStream) {
        quint8 t = field ? 0xff : 0;
        binStream->write(&t, 1);
    }

    template <class T>
    void serialize(quint8 field[], QnOutputBinaryStream<T>* binStream) {
        binStream->write(&field, sizeof(field));
    }

    template <class T>
    void serialize(const QByteArray& field, QnOutputBinaryStream<T>* binStream) {
        serialize((qint32) field.size(), binStream);
        binStream->write(field.data(), field.size());
    }

    template <class T>
    void serialize(const QString& field, QnOutputBinaryStream<T>* binStream) {
        serialize(field.toUtf8(), binStream);
    }

    template <class T>
    void serialize(const QUrl& field, QnOutputBinaryStream<T>* binStream) {
        serialize(field.toString(), binStream);
    }

    template <class T, class T2>
    void serialize(const std::vector<T2>& field, QnOutputBinaryStream<T>* binStream);

    template <class T, class T2>
    void serialize(const QList<T2>& field, QnOutputBinaryStream<T>* binStream);

    template <class T, class T2>
    void serialize(const QSet<T2>& field, QnOutputBinaryStream<T>* binStream);

    template <class T, class T2, class T3>
    void serialize(const QMap<T2, T3>& field, QnOutputBinaryStream<T>* binStream);

    // -------------------- deserialize ---------------------

    template <class T>
    bool deserialize(QnId& field, QnInputBinaryStream<T>* binStream) {
        QByteArray data;
        data.resize(16);
        if( binStream->read(data.data(), 16) != 16 )
            return false;
        field = QUuid::fromRfc4122(data);
        return true;
    }

    template <class T>
    bool deserialize(qint32& field, QnInputBinaryStream<T>* binStream) {
        qint32 tmp;
        if( binStream->read(&tmp, sizeof(field)) != sizeof(field) )
            return false;
        field = ntohl(tmp);
        return true;
    }

    template <class T>
    bool deserialize(quint32& field, QnInputBinaryStream<T>* binStream) {
        quint32 tmp;
        if( binStream->read(&tmp, sizeof(field)) != sizeof(field) )
            return false;
        field = ntohl(tmp);
        return true;
    }

    template <class T>
    bool deserialize(qint16& field, QnInputBinaryStream<T>* binStream) {
        qint16 tmp;
        if( binStream->read(&tmp, sizeof(field)) != sizeof(field) )
            return false;
        field = ntohs(tmp);
        return true;
    }

    template <class T>
    bool deserialize(quint16& field, QnInputBinaryStream<T>* binStream) {
        quint16 tmp;
        if( binStream->read(&tmp, sizeof(field)) != sizeof(field) )
            return false;
        field = ntohs(tmp);
        return true;
    }

    template <class T>
    bool deserialize(qint8& field, QnInputBinaryStream<T>* binStream) {
        if( binStream->read(&field, sizeof(field)) != sizeof(field) )
            return false;
        return true;
    }

    template <class T>
    bool deserialize(quint8& field, QnInputBinaryStream<T>* binStream) {
        if( binStream->read(&field, sizeof(field)) != sizeof(field) )
            return false;
        return true;
    }

    template <class T>
    bool deserialize(qint64& field, QnInputBinaryStream<T>* binStream) {
        qint64 tmp;
        if( binStream->read(&tmp, sizeof(field)) != sizeof(field) )
            return false;
        field = ntohll(tmp);
        return true;
    }

    template <class T>
    bool deserialize(float& field, QnInputBinaryStream<T>* binStream) {
        return binStream->read(&field, sizeof(field)) == sizeof(field);
    }

    template <class T>
    bool deserialize(double& field, QnInputBinaryStream<T>* binStream) {
        return binStream->read(&field, sizeof(field)) == sizeof(field);
    }

    template <class T>
    bool deserialize(bool& field, QnInputBinaryStream<T>* binStream) {
        quint8 t;
        bool rez = binStream->read(&t, 1) == 1;
        field = (bool) t;
        return rez;
    }

    template <std::size_t N, class T>
    bool deserialize(quint8(&field)[N], QnInputBinaryStream<T>* binStream) {
        return binStream->read(&field, sizeof(field)) == sizeof(field);
    }

    template <class T>
    bool deserialize(QByteArray& field, QnInputBinaryStream<T>* binStream) {
        qint32 size;
        if( !deserialize(size, binStream) )
            return false;
        field.resize(size);
        return binStream->read(field.data(), size) == size;
    }

    template <class T>
    bool deserialize(QString& field, QnInputBinaryStream<T>* binStream) {
        QByteArray data;
        if( !deserialize(data, binStream) )
            return false;
        field = QString::fromUtf8(data);
        return true;
    }

    template <class T>
    bool deserialize(QUrl& field, QnInputBinaryStream<T>* binStream) {
        QByteArray data;
        if( !deserialize(data, binStream) )
            return false;
        field = QUrl(QString::fromUtf8(data));
        return true;
    }

    template<class T, class T2>
        bool deserialize( T2& field, QnInputBinaryStream<T>* binStream, typename std::enable_if<std::is_enum<T2>::value>::type* = NULL )
    {
        qint32 tmp;
        if( binStream->read(&tmp, sizeof(field)) != sizeof(field) )
            return false;
        field = (T2) ntohl(tmp);
        return true;
    }

    template <class T, class T2>
    void serialize(const std::vector<T2>& field, QnOutputBinaryStream<T>* binStream) 
    {
        serialize((qint32) field.size(), binStream);
        for (typename std::vector<T2>::const_iterator itr = field.begin(); itr != field.end(); ++itr)
            serialize(*itr, binStream);
    }

    template <class T, class T2>
    bool deserialize(std::vector<T2>& field, QnInputBinaryStream<T>* binStream) 
    {
        qint32 size;
        if( !deserialize(size, binStream) )
            return false;
        field.resize(size);
        for( T2& val: field )
            if( !deserialize(val, binStream) )
                return false;
        return true;
    }

    template <class T, class K, class V>
    void serialize(const std::map<K, V>& field, QnOutputBinaryStream<T>* binStream) 
    {
        serialize((qint32) field.size(), binStream);
        for(typename std::map<K, V>::const_iterator itr = field.begin(); itr != field.end(); ++itr) {
            serialize(itr->first, binStream);
            serialize(itr->second, binStream);
        }
    }

    template <class T, class K, class V>
    bool deserialize(std::map<K, V>& field, QnInputBinaryStream<T>* binStream) 
    {
        qint32 size = 0;
        if( !deserialize(size, binStream) )
            return false;
        for( qint32 i = 0; i < size; ++i )
        {
            K key;
            V value;
            if( !deserialize(key, binStream) || !deserialize(value, binStream))
                return false;
            field.insert(std::pair<K, V>(key, value));
        }
        return true;
    }

    template <class T, class T2>
    void serialize(const QList<T2>& field, QnOutputBinaryStream<T>* binStream) 
    {
        serialize((qint32) field.size(), binStream);
        using namespace std::placeholders;
        std::for_each( field.begin(), field.end(), [binStream](const T2& val){ serialize(val, binStream); } );
    }

    template <class T, class T2>
    void serialize(const QSet<T2>& field, QnOutputBinaryStream<T>* binStream) 
    {
        serialize((qint32) field.size(), binStream);
        using namespace std::placeholders;
        std::for_each( field.begin(), field.end(), [binStream](const T2& val){ serialize(val, binStream); } );
    }

    template <class T, class T2, class T3>
    void serialize(const QMap<T2, T3>& field, QnOutputBinaryStream<T>* binStream) 
    {
        serialize((qint32) field.size(), binStream);
        for(typename QMap<T2, T3>::const_iterator itr = field.begin(); itr != field.end(); ++itr) {
            serialize(itr.key(), binStream);
            serialize(itr.value(), binStream);
        }
    }

    template <class T, class T2>
    bool deserialize(QList<T2>& field, QnInputBinaryStream<T>* binStream) 
    {
        qint32 size = 0;
        if( !deserialize(size, binStream) )
            return false;
        for( qint32 i = 0; i < size; ++i )
        {
            field.push_back( T2() );
            if( !deserialize(field.back(), binStream) )
                return false;
        }
        return true;
    }

    template <class T, class T2>
    bool deserialize(QSet<T2>& field, QnInputBinaryStream<T>* binStream) 
    {
        qint32 size = 0;
        if( !deserialize(size, binStream) )
            return false;
        for( qint32 i = 0; i < size; ++i )
        {
            T2 t2;
            if( !deserialize(t2, binStream) )
                return false;
            field.insert( t2 );
        }
        return true;
    }

    template <class T, class T2, class T3>
    bool deserialize(QMap<T2, T3>& field, QnInputBinaryStream<T>* binStream) 
    {
        qint32 size = 0;
        if( !deserialize(size, binStream) )
            return false;
        for( qint32 i = 0; i < size; ++i )
        {
            T2 t2;
            T3 t3;
            if( !deserialize(t2, binStream) || !deserialize(t3, binStream))
                return false;
            field.insert(t2, t3);
        }
        return true;
    }

//}

#ifndef Q_MOC_RUN

#define QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS(TYPE, FIELD_SEQ, ... /* PREFIX */) \
    template <class T> \
    __VA_ARGS__ void serialize(const TYPE &value, QnOutputBinaryStream<T> *target) { \
       BOOST_PP_SEQ_FOR_EACH(SERIALIZE_FIELD, ~, FIELD_SEQ) \
    } \
    \
    template <class T> \
    __VA_ARGS__ bool deserialize(TYPE &value, QnInputBinaryStream<T> *target) { \
       BOOST_PP_SEQ_FOR_EACH(DESERIALIZE_FIELD, ~, FIELD_SEQ) \
       return true; \
    } 

#define SERIALIZE_FIELD(R, D, FIELD) \
    serialize(value.FIELD, target); \

#define DESERIALIZE_FIELD(R, D, FIELD) \
    if( !deserialize(value.FIELD, target) ) \
        return false;


/*
#define QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS(TYPE, BASE_TYPE, FIELD_SEQ, ... ) \
    QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS(TYPE, FIELD_SEQ); \
    template <class T> \
    void TYPE::serialize(OutputBinaryStream<T>* stream) const \
{ \
    BASE_TYPE::serialize(stream); \
    serialize(*this, stream); \
} \
    \
    template <class T> \
    bool TYPE::deserialize(InputBinaryStream<T>* stream) \
{ \
    return BASE_TYPE::deserialize(stream) && \
           deserialize(*this, stream); \
}
*/

#define QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS(TYPE, BASE_TYPE, FIELD_SEQ, ... ) \
    template <class T> \
    __VA_ARGS__ void serialize(const TYPE &value, QnOutputBinaryStream<T> *target) { \
        serialize((const BASE_TYPE&)value, target);  \
        BOOST_PP_SEQ_FOR_EACH(SERIALIZE_FIELD, ~, FIELD_SEQ) \
    } \
    \
    template <class T> \
    __VA_ARGS__ bool deserialize(TYPE &value, QnInputBinaryStream<T> *target) { \
        if( !deserialize((BASE_TYPE&)value, target))  \
            return false; \
        BOOST_PP_SEQ_FOR_EACH(DESERIALIZE_FIELD, ~, FIELD_SEQ) \
        return true; \
    }


#define QN_DEFINE_STRUCT_SERIALIZATORS(TYPE, FIELD_SEQ, ... /* PREFIX */) \
    QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS(TYPE, FIELD_SEQ); 

#define QN_DEFINE_API_OBJECT_LIST_DATA(TYPE) \
    struct TYPE ## ListData: public ApiData { \
        std::vector<TYPE> data; \
    }; \
    QN_DEFINE_STRUCT_SERIALIZATORS (TYPE ## ListData, (data) )

#else // Q_MOC_RUN

/* Qt moc chokes on our macro hell, so we make things easier for it. */
#define QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS(...)
#define QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS(...)
#define QN_DEFINE_STRUCT_SERIALIZATORS(...)
#define QN_DEFINE_API_OBJECT_LIST_DATA(...)

#endif // Q_MOC_RUN

#endif

#endif // QN_SERIALIZATION_BINARY_H
