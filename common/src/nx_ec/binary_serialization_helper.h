
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

    int read(void* buffer, int maxSize) const {
        int toRead = qMin(m_data.size() - pos, maxSize);
        memcpy(buffer, m_data.constData() + pos, toRead);
        pos += toRead;
        return toRead;
    }
private:
    const QByteArray& m_data;
    mutable int pos;
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
        binStream->write(&field, sizeof(field));
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

    template <class T, class T2>
    void serialize(const std::vector<T2>& field, OutputBinaryStream<T>* binStream);

    // -------------------- deserialize ---------------------

    template <class T>
    void deserialize(qint32& field, const InputBinaryStream<T>* binStream) {
        qint32 tmp;
        binStream->read(&tmp, sizeof(field));
        field = ntohl(tmp);
    }

    template <class T>
    void deserialize(quint32& field, const InputBinaryStream<T>* binStream) {
        quint32 tmp;
        binStream->read(&tmp, sizeof(field));
        field = ntohl(tmp);
    }

    template <class T>
    void deserialize(qint16& field, const InputBinaryStream<T>* binStream) {
        qint16 tmp;
        binStream->read(&tmp, sizeof(field));
        field = ntohs(tmp);
    }

    template <class T>
    void deserialize(quint16& field, const InputBinaryStream<T>* binStream) {
        quint16 tmp;
        binStream->read(&tmp, sizeof(field));
        field = ntohs(tmp);
    }

    template <class T>
    void deserialize(qint64& field, const InputBinaryStream<T>* binStream) {
        qint64 tmp;
        binStream->read(&tmp, sizeof(field));
        field = ntohll(tmp);
    }

    template <class T>
    void deserialize(float& field, const InputBinaryStream<T>* binStream) {
        binStream->read(&field, sizeof(field));
    }

    template <class T>
    void deserialize(double& field, const InputBinaryStream<T>* binStream) {
        binStream->read(&field, sizeof(field));
    }

    template <class T>
    void deserialize(bool& field, const InputBinaryStream<T>* binStream) {
        binStream->read(&field, sizeof(field));
    }

    typedef quint8 FixedArray[];
    template <class T>
    void deserialize(FixedArray& field, const InputBinaryStream<T>* binStream) {
        binStream->read(&field, sizeof(field));
    }

    template <class T>
    void deserialize(QByteArray& field, const InputBinaryStream<T>* binStream) {
        qint32 size;
        deserialize(size, binStream);
        field.resize(size);
        binStream->read(field.data(), size);
    }

    template <class T>
    void deserialize(QString& field, const InputBinaryStream<T>* binStream) {
        QByteArray data;
        deserialize(data, binStream);
        field = QString::fromUtf8(data);
    }

    template<class T, class T2>
        void deserialize( T2& field, const InputBinaryStream<T>* binStream, typename std::enable_if<std::is_enum<T2>::value>::type* = NULL )
    {
        qint32 tmp;
        binStream->read(&tmp, sizeof(field));
        field = (T2) ntohl(tmp);
    }


};

#define QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS(TYPE, FIELD_SEQ, ... /* PREFIX */) \
namespace QnBinary { \
    template <class T> \
    __VA_ARGS__ void serialize(const TYPE &value, OutputBinaryStream<T> *target) { \
       BOOST_PP_SEQ_FOR_EACH(SERIALIZE_FIELD, ~, FIELD_SEQ) \
    } \
    \
    template <class T> \
    __VA_ARGS__ void deserialize(TYPE &value, const InputBinaryStream<T> *target) { \
       BOOST_PP_SEQ_FOR_EACH(DESERIALIZE_FIELD, ~, FIELD_SEQ) \
    } \
}


#define SERIALIZE_FIELD(R, D, FIELD) \
    QnBinary::serialize(value.FIELD, target); \

#define DESERIALIZE_FIELD(R, D, FIELD) \
    QnBinary::deserialize(value.FIELD, target);


namespace QnBinary
{
    template <class T, class T2>
    void serialize(const std::vector<T2>& field, OutputBinaryStream<T>* binStream) 
    {
        QnBinary::serialize((qint32) field.size(), binStream);
        for (std::vector<T2>::const_iterator itr = field.begin(); itr != field.end(); ++itr)
            QnBinary::serialize(*itr, binStream);
    }

    template <class T, class T2>
    void deserialize(std::vector<T2>& field, const InputBinaryStream<T>* binStream) 
    {
        qint32 size;
        deserialize(size, binStream);
        field.resize(size);
        std::for_each( field.begin(), field.end(), [binStream](T2& val){ QnBinary::deserialize(val, binStream); } );
    }
}

#define QN_DEFINE_DERIVED_STRUCT_SERIALIZATORS(TYPE, BASE_TYPE, FIELD_SEQ, ... /* PREFIX */) \
    QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS(TYPE, FIELD_SEQ); \
    template <class T> \
    void TYPE::serialize(OutputBinaryStream<T>& stream) const \
{ \
    BASE_TYPE::serialize(stream); \
    QnBinary::serialize(*this, &stream); \
} \
    \
    template <class T> \
    void TYPE::deserialize(const InputBinaryStream<T>& stream) \
{ \
    BASE_TYPE::deserialize(stream); \
    QnBinary::deserialize(*this, &stream); \
}

#define QN_DEFINE_STRUCT_SERIALIZATORS(TYPE, FIELD_SEQ, ... /* PREFIX */) \
    QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS(TYPE, FIELD_SEQ); \
    template <class T> \
    void TYPE::serialize(OutputBinaryStream<T>& stream) const \
{ \
    QnBinary::serialize(*this, &stream); \
} \
    \
    template <class T> \
    void TYPE::deserialize(const InputBinaryStream<T>& stream) \
{ \
    QnBinary::deserialize(*this, &stream); \
} \


#define QN_DECLARE_STRUCT_SERIALIZATORS() \
    template <class T> void serialize(OutputBinaryStream<T>& stream) const; \
    template <class T> void deserialize(const InputBinaryStream<T>& stream); \



#endif  //SERIALIZATION_HELPER_H
