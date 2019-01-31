#ifndef QN_SERIALIZATION_COMPRESSED_TIME_MACROS_H
#define QN_SERIALIZATION_COMPRESSED_TIME_MACROS_H

#include <utility> /* For std::move. */

#include "compressed_time.h"

#include <nx/fusion/fusion/fusion_serialization.h>

namespace QnCompressedTimeDetail {
    template<class Output>
    class SerializationVisitor {
    public:
        SerializationVisitor(QnCompressedTimeWriter<Output> *stream):
            m_stream(stream)
        {}


        template<class T, class Access>
        bool operator()(const T &value, const Access &access) const {
            using namespace QnFusion;
            QnCompressedTime::serialize(invoke(access(getter), value), m_stream);
            return true;
        }

    private:
        QnCompressedTimeWriter<Output> *m_stream;
    };

    template<class Input>
    struct DeserializationVisitor {
    public:
        DeserializationVisitor(QnCompressedTimeReader<Input> *stream):
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

            return QnCompressedTime::deserialize(m_stream, &(target.*access(setter)));
        }

        template<class T, class Access, class Member>
        bool operator()(T &target, const Access &access, const QnFusion::typed_function_setter_tag<Member> &) {
            using namespace QnFusion;

            Member member;
            if(!QnCompressedTime::deserialize(m_stream, &member))
                return false;
            invoke(access(setter), target, std::move(member));
            return true;
        }

    private:
        QnCompressedTimeReader<Input> *m_stream;
    };

} // namespace QnBinaryDetail


QN_FUSION_REGISTER_SERIALIZATION_VISITOR_TPL((class Output), (QnCompressedTimeWriter<Output> *), (QnCompressedTimeDetail::SerializationVisitor<Output>))
QN_FUSION_REGISTER_DESERIALIZATION_VISITOR_TPL((class Input), (QnCompressedTimeReader<Input> *), (QnCompressedTimeDetail::DeserializationVisitor<Input>))


#define QN_FUSION_DEFINE_FUNCTIONS_compressed_time(TYPE, ... /* PREFIX */)               \
__VA_ARGS__ void serialize(const TYPE &value, QnCompressedTimeWriter<QByteArray> *stream) { \
    QnFusion::serialize(value, &stream);                                        \
}                                                                               \
                                                                                \
__VA_ARGS__ bool deserialize(QnCompressedTimeReader<QByteArray> *stream, TYPE *target) { \
    return QnFusion::deserialize(stream, target);                               \
}


#endif // QN_SERIALIZATION_COMPRESSED_TIME_MACROS_H
