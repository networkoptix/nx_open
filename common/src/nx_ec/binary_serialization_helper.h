
#ifndef SERIALIZATION_HELPER_H
#define SERIALIZATION_HELPER_H

#include <QByteArray>
#include <QIODevice>
#include <QString>
#include <QUuid>

#include <type_traits>

#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/transform.hpp>
#include <boost/preprocessor/tuple/enum.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/type_traits/remove_reference.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/mpl/bool.hpp>
#include "utils/common/id.h"
#include "utils/math/math.h"


//!Stream providing read operation
template<class T>
class InputBinaryStream;

//!Stream providing write operation
template<class T>
class OutputBinaryStream;

template<>
class InputBinaryStream<QIODevice> {
};

template<>
class OutputBinaryStream<QIODevice> {
};

template<>
class InputBinaryStream<QByteArray> {
public:
    InputBinaryStream<QByteArray>( const QByteArray& data )
    :
        m_data( data ),
        pos( 0 )
    {
    }

    /*!
        \return Bytes actually read
    */
    int read(void* buffer, int maxSize) {
        int toRead = qMin(m_data.size() - pos, maxSize);
        memcpy(buffer, m_data.constData() + pos, toRead);
        pos += toRead;
        return toRead;
    }

    //!Resets internal cursor position
    void reset()
    {
        pos = 0;
    }

    const QByteArray& buffer() const { return m_data; }
    int getPos() const { return pos; }

private:
    const QByteArray& m_data;
    int pos;
};

template<>
class OutputBinaryStream<QByteArray> {
public:
    OutputBinaryStream<QByteArray>( QByteArray* const data )
    :
        m_data( data )
    {
    }

    int write(const void* data, int size) {
        m_data->append((const char*) data, size);
        return size;
    }

private:
    QByteArray* const m_data;
};


namespace QnBinary {
    template <class T>
    void serialize(const QnId& field, OutputBinaryStream<T>* binStream) {
        QByteArray data = field.toRfc4122();
        binStream->write(data.data(), data.size());
    }
    template <class T>
    void serialize(qint32 field, OutputBinaryStream<T>* binStream) {
        qint32 tmp = htonl(field);
        binStream->write(&tmp, sizeof(tmp));
    }

    template <class T>
    void serialize(quint32 field, OutputBinaryStream<T>* binStream) {
        quint32 tmp = htonl(field);
        binStream->write(&tmp, sizeof(tmp));
    }

    template <class T>
    void serialize(qint16 field, OutputBinaryStream<T>* binStream) {
        qint16 tmp = htons(field);
        binStream->write(&tmp, sizeof(tmp));
    }

    template <class T>
    void serialize(quint16 field, OutputBinaryStream<T>* binStream) {
        quint16 tmp = htons(field);
        binStream->write(&tmp, sizeof(tmp));
    }

    template <class T>
    void serialize(qint8 field, OutputBinaryStream<T>* binStream) {
        binStream->write(&field, sizeof(field));
    }

    template <class T>
    void serialize(quint8 field, OutputBinaryStream<T>* binStream) {
        binStream->write(&field, sizeof(field));
    }

    template <class T>
    void serialize(qint64 field, OutputBinaryStream<T>* binStream) {
        qint64 tmp = htonll(field);
        binStream->write(&tmp, sizeof(field));
    }

    template <class T>
    void serialize(float field, OutputBinaryStream<T>* binStream) {
        binStream->write(&field, sizeof(field));
    }

    template <class T>
    void serialize(double field, OutputBinaryStream<T>* binStream) {
        binStream->write(&field, sizeof(field));
    }

    template <class T>
    void serialize(bool field, OutputBinaryStream<T>* binStream) {
        quint8 t = field ? 0xff : 0;
        binStream->write(&t, 1);
    }

    template <class T>
    void serialize(quint8 field[], OutputBinaryStream<T>* binStream) {
        binStream->write(&field, sizeof(field));
    }

    template <class T>
    void serialize(const QByteArray& field, OutputBinaryStream<T>* binStream) {
        serialize((qint32) field.size(), binStream);
        binStream->write(field.data(), field.size());
    }

    template <class T>
    void serialize(const QString& field, OutputBinaryStream<T>* binStream) {
        serialize(field.toUtf8(), binStream);
    }

    template <class T>
    void serialize(const QUrl& field, OutputBinaryStream<T>* binStream) {
        serialize(field.toString(), binStream);
    }

    template <class T, class T2>
    void serialize(const std::vector<T2>& field, OutputBinaryStream<T>* binStream);

    template <class T, class T2>
    void serialize(const QList<T2>& field, OutputBinaryStream<T>* binStream);

    template <class T, class T2>
    void serialize(const QSet<T2>& field, OutputBinaryStream<T>* binStream);

    template <class T, class T2, class T3>
    void serialize(const QMap<T2, T3>& field, OutputBinaryStream<T>* binStream);

    // -------------------- deserialize ---------------------

    template <class T>
    bool deserialize(QnId& field, InputBinaryStream<T>* binStream) {
        QByteArray data;
        data.resize(16);
        if( binStream->read(data.data(), 16) != 16 )
            return false;
        field = QUuid::fromRfc4122(data);
        return true;
    }

    template <class T>
    bool deserialize(qint32& field, InputBinaryStream<T>* binStream) {
        qint32 tmp;
        if( binStream->read(&tmp, sizeof(field)) != sizeof(field) )
            return false;
        field = ntohl(tmp);
        return true;
    }

    template <class T>
    bool deserialize(quint32& field, InputBinaryStream<T>* binStream) {
        quint32 tmp;
        if( binStream->read(&tmp, sizeof(field)) != sizeof(field) )
            return false;
        field = ntohl(tmp);
        return true;
    }

    template <class T>
    bool deserialize(qint16& field, InputBinaryStream<T>* binStream) {
        qint16 tmp;
        if( binStream->read(&tmp, sizeof(field)) != sizeof(field) )
            return false;
        field = ntohs(tmp);
        return true;
    }

    template <class T>
    bool deserialize(quint16& field, InputBinaryStream<T>* binStream) {
        quint16 tmp;
        if( binStream->read(&tmp, sizeof(field)) != sizeof(field) )
            return false;
        field = ntohs(tmp);
        return true;
    }

    template <class T>
    bool deserialize(qint8& field, InputBinaryStream<T>* binStream) {
        if( binStream->read(&field, sizeof(field)) != sizeof(field) )
            return false;
        return true;
    }

    template <class T>
    bool deserialize(quint8& field, InputBinaryStream<T>* binStream) {
        if( binStream->read(&field, sizeof(field)) != sizeof(field) )
            return false;
        return true;
    }

    template <class T>
    bool deserialize(qint64& field, InputBinaryStream<T>* binStream) {
        qint64 tmp;
        if( binStream->read(&tmp, sizeof(field)) != sizeof(field) )
            return false;
        field = ntohll(tmp);
        return true;
    }

    template <class T>
    bool deserialize(float& field, InputBinaryStream<T>* binStream) {
        return binStream->read(&field, sizeof(field)) == sizeof(field);
    }

    template <class T>
    bool deserialize(double& field, InputBinaryStream<T>* binStream) {
        return binStream->read(&field, sizeof(field)) == sizeof(field);
    }

    template <class T>
    bool deserialize(bool& field, InputBinaryStream<T>* binStream) {
        quint8 t;
        bool rez = binStream->read(&t, 1) == 1;
        field = (bool) t;
        return rez;
    }

    template <std::size_t N, class T>
    bool deserialize(quint8(&field)[N], InputBinaryStream<T>* binStream) {
        return binStream->read(&field, sizeof(field)) == sizeof(field);
    }

    template <class T>
    bool deserialize(QByteArray& field, InputBinaryStream<T>* binStream) {
        qint32 size;
        if( !deserialize(size, binStream) )
            return false;
        field.resize(size);
        return binStream->read(field.data(), size) == size;
    }

    template <class T>
    bool deserialize(QString& field, InputBinaryStream<T>* binStream) {
        QByteArray data;
        if( !deserialize(data, binStream) )
            return false;
        field = QString::fromUtf8(data);
        return true;
    }

    template <class T>
    bool deserialize(QUrl& field, InputBinaryStream<T>* binStream) {
        QByteArray data;
        if( !deserialize(data, binStream) )
            return false;
        field = QUrl(QString::fromUtf8(data));
        return true;
    }

    template<class T, class T2>
        bool deserialize( T2& field, InputBinaryStream<T>* binStream, typename std::enable_if<std::is_enum<T2>::value>::type* = NULL )
    {
        qint32 tmp;
        if( binStream->read(&tmp, sizeof(field)) != sizeof(field) )
            return false;
        field = (T2) ntohl(tmp);
        return true;
    }

    template <class T, class T2>
    void serialize(const std::vector<T2>& field, OutputBinaryStream<T>* binStream) 
    {
        QnBinary::serialize((qint32) field.size(), binStream);
        for (typename std::vector<T2>::const_iterator itr = field.begin(); itr != field.end(); ++itr)
            QnBinary::serialize(*itr, binStream);
    }

    template <class T, class T2>
    bool deserialize(std::vector<T2>& field, InputBinaryStream<T>* binStream) 
    {
        qint32 size;
        if( !deserialize(size, binStream) )
            return false;
        field.resize(size);
        for( T2& val: field )
            if( !QnBinary::deserialize(val, binStream) )
                return false;
        return true;
    }

    template <class T, class T2>
    void serialize(const QList<T2>& field, OutputBinaryStream<T>* binStream) 
    {
        QnBinary::serialize((qint32) field.size(), binStream);
        using namespace std::placeholders;
        std::for_each( field.begin(), field.end(), [binStream](const T2& val){ QnBinary::serialize(val, binStream); } );
    }

    template <class T, class T2>
    void serialize(const QSet<T2>& field, OutputBinaryStream<T>* binStream) 
    {
        QnBinary::serialize((qint32) field.size(), binStream);
        using namespace std::placeholders;
        std::for_each( field.begin(), field.end(), [binStream](const T2& val){ QnBinary::serialize(val, binStream); } );
    }

    template <class T, class T2, class T3>
    void serialize(const QMap<T2, T3>& field, OutputBinaryStream<T>* binStream) 
    {
        QnBinary::serialize((qint32) field.size(), binStream);
        for(typename QMap<T2, T3>::const_iterator itr = field.begin(); itr != field.end(); ++itr) {
            QnBinary::serialize(itr.key(), binStream);
            QnBinary::serialize(itr.value(), binStream);
        }
    }

    template <class T, class T2>
    bool deserialize(QList<T2>& field, InputBinaryStream<T>* binStream) 
    {
        qint32 size = 0;
        if( !deserialize(size, binStream) )
            return false;
        for( qint32 i = 0; i < size; ++i )
        {
            field.push_back( T2() );
            if( !QnBinary::deserialize(field.back(), binStream) )
                return false;
        }
        return true;
    }

    template <class T, class T2>
    bool deserialize(QSet<T2>& field, InputBinaryStream<T>* binStream) 
    {
        qint32 size = 0;
        if( !deserialize(size, binStream) )
            return false;
        for( qint32 i = 0; i < size; ++i )
        {
            T2 t2;
            if( !QnBinary::deserialize(t2, binStream) )
                return false;
            field.insert( t2 );
        }
        return true;
    }

    template <class T, class T2, class T3>
    bool deserialize(QMap<T2, T3>& field, InputBinaryStream<T>* binStream) 
    {
        qint32 size = 0;
        if( !deserialize(size, binStream) )
            return false;
        for( qint32 i = 0; i < size; ++i )
        {
            T2 t2;
            T3 t3;
            if( !QnBinary::deserialize(t2, binStream) || !QnBinary::deserialize(t3, binStream))
                return false;
            field.insert(t2, t3);
        }
        return true;
    }

}

#ifndef Q_MOC_RUN

#define QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS(TYPE, FIELD_SEQ, ... /* PREFIX */) \
namespace QnBinary { \
    template <class T> \
    __VA_ARGS__ void serialize(const TYPE &value, OutputBinaryStream<T> *target) { \
       BOOST_PP_SEQ_FOR_EACH(SERIALIZE_FIELD, ~, FIELD_SEQ) \
    } \
    \
    template <class T> \
    __VA_ARGS__ bool deserialize(TYPE &value, InputBinaryStream<T> *target) { \
       BOOST_PP_SEQ_FOR_EACH(DESERIALIZE_FIELD, ~, FIELD_SEQ) \
       return true; \
    } \
}

#define SERIALIZE_FIELD(R, D, FIELD) \
    QnBinary::serialize(value.FIELD, target); \

#define DESERIALIZE_FIELD(R, D, FIELD) \
    if( !QnBinary::deserialize(value.FIELD, target) ) \
        return false;


/*
#define QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS(TYPE, BASE_TYPE, FIELD_SEQ, ... ) \
    QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS(TYPE, FIELD_SEQ); \
    template <class T> \
    void TYPE::serialize(OutputBinaryStream<T>* stream) const \
{ \
    BASE_TYPE::serialize(stream); \
    QnBinary::serialize(*this, stream); \
} \
    \
    template <class T> \
    bool TYPE::deserialize(InputBinaryStream<T>* stream) \
{ \
    return BASE_TYPE::deserialize(stream) && \
           QnBinary::deserialize(*this, stream); \
}
*/

#define QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS(TYPE, BASE_TYPE, FIELD_SEQ, ... ) \
    namespace QnBinary { \
    template <class T> \
    __VA_ARGS__ void serialize(const TYPE &value, OutputBinaryStream<T> *target) { \
    QnBinary::serialize((const BASE_TYPE&)value, target);  \
    BOOST_PP_SEQ_FOR_EACH(SERIALIZE_FIELD, ~, FIELD_SEQ) \
} \
    \
    template <class T> \
    __VA_ARGS__ bool deserialize(TYPE &value, InputBinaryStream<T> *target) { \
    if( !QnBinary::deserialize((BASE_TYPE&)value, target))  \
    return false; \
    BOOST_PP_SEQ_FOR_EACH(DESERIALIZE_FIELD, ~, FIELD_SEQ) \
    return true; \
} \
}


#define QN_DEFINE_STRUCT_SERIALIZATORS(TYPE, FIELD_SEQ, ... /* PREFIX */) \
    QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS(TYPE, FIELD_SEQ); 

#else // Q_MOC_RUN

/* Qt moc chokes on our macro hell, so we make things easier for it. */
#define QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS(...)
#define QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS(...)
#define QN_DEFINE_STRUCT_SERIALIZATORS(...)

#endif // Q_MOC_RUN

#endif  //SERIALIZATION_HELPER_H
