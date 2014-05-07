#ifndef QN_SERIALIZATION_BINARY_H
#define QN_SERIALIZATION_BINARY_H

#include <utils/serialization/serialization.h>
#include <utils/fusion/fusion_serialization.h>

#include "binary_fwd.h"
#include "binary_stream.h"

namespace QnBinary {
    template<class T, class Output>
    void serialize(const T &value, QnOutputBinaryStream<Output> *stream) {
        QnSerialization::serialize(value, stream);
    }

    template<class T, class Input>
    bool deserialize(QnInputBinaryStream<Input> *stream, T *target) {
        return QnSerialization::deserialize(stream, target);
    }

#ifndef QN_NO_QT
    template<class T>
    QByteArray serialized(const T &value) {
        QByteArray result;
        QnOutputBinaryStream<QByteArray> stream(&result);
        QnBinary::serialize(value, &stream);
        return result;
    }

    template<class T>
    T deserialized(const QByteArray &value, const T &defaultValue = T(), bool *success = NULL) {
        T target;
        QnInputBinaryStream<QByteArray> stream(value);
        bool result = QnBinary::deserialize(&stream, &target);
        if (success)
            *success = result;
        return result ? target : defaultValue;
    }
#endif


} // namespace QnBinary


namespace QnBinaryDetail {
    template<class Output>
    class SerializationVisitor {
    public:
        SerializationVisitor(QnOutputBinaryStream<Output> *stream): 
            m_stream(stream) 
        {}

        template<class T, class Access>
        bool operator()(const T &, const Access &access, const QnFusion::start_tag &) const {
            using namespace QnFusion;

            typedef decltype(access(member_count)) member_count_type;
            static_assert(member_count_type::value < 32, "Only structures with up to 32 members are supported.");

            /* Write out 1-byte header:
             * 
             * 0 .... 2  3 ........ 7
             * Reserved  Member Count */
            QnBinary::serialize(static_cast<unsigned char>(access(member_count)), m_stream);
            return true;
        }

        template<class T, class Access>
        bool operator()(const T &value, const Access &access) const {
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
        bool operator()(const T &, const Access &access, const QnFusion::start_tag &) {
            using namespace QnFusion;

            unsigned char header;
            QnBinary::deserialize(m_stream, &header);
            if(header & 0xE0)
                return false; /* Reserved fields are expected to be zero. A packet from next version? */

            m_count = header;
            if(access(member_count) < m_count)
                return false; /* Looks like a packet from the next version. */
            
            return true;
        }

        template<class T, class Access>
        bool operator()(T &target, const Access &access) {
            using namespace QnFusion;

            // TODO: #Elric 
            // we need a global "walker" abstraction to implement this more efficiently.
            if(access(member_index) >= m_count)
                return true;

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
        int m_count;
    };

} // namespace QnBinaryDetail


QN_FUSION_REGISTER_SERIALIZATION_VISITOR(QnOutputBinaryStream<QByteArray> *, QnBinaryDetail::SerializationVisitor<QByteArray>)
QN_FUSION_REGISTER_DESERIALIZATION_VISITOR(QnInputBinaryStream<QByteArray> *, QnBinaryDetail::DeserializationVisitor<QByteArray>)


#define QN_FUSION_DEFINE_FUNCTIONS_binary(TYPE, ... /* PREFIX */)               \
__VA_ARGS__ void serialize(const TYPE &value, QnOutputBinaryStream<QByteArray> *stream) { \
    QnFusion::serialize(value, &stream);                                        \
}                                                                               \
                                                                                \
__VA_ARGS__ bool deserialize(QnInputBinaryStream<QByteArray> *stream, TYPE *target) { \
    return QnFusion::deserialize(stream, target);                               \
}


#endif // QN_SERIALIZATION_BINARY_H
