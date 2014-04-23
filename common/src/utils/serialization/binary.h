#ifndef QN_SERIALIZATION_BINARY_H
#define QN_SERIALIZATION_BINARY_H

#include <QtCore/QByteArray>

#include <utils/serialization/serialization.h>
#include <utils/fusion/fusion_serialization.h>

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


QN_FUSION_REGISTER_SERIALIZATION_VISITOR(QnOutputBinaryStream<QByteArray> *, QnBinaryDetail::SerializationVisitor<QByteArray>)
QN_FUSION_REGISTER_DESERIALIZATION_VISITOR(QnInputBinaryStream<QByteArray> *, QnBinaryDetail::DeserializationVisitor<QByteArray>)


#define QN_FUSION_DEFINE_FUNCTIONS_binary(TYPE, ... /* PREFIX */)               \
__VA_ARGS__ void serialize(const TYPE &value, QnOutputBinaryStream<QByteArray> *stream) { \
    QnFusion::serialize(value, stream);                                         \
}                                                                               \
                                                                                \
__VA_ARGS__ bool deserialize(QnInputBinaryStream<QByteArray> *stream, TYPE *target) { \
    return QnFusion::deserialize(stream, target);                               \
}



#if 0

#define QN_DEFINE_API_OBJECT_LIST_DATA(TYPE) \
    struct TYPE ## ListData: public ApiData { \
        std::vector<TYPE> data; \
    }; \
    QN_DEFINE_STRUCT_SERIALIZATORS (TYPE ## ListData, (data) )

#endif

#endif // QN_SERIALIZATION_BINARY_H
