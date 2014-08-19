#ifndef QN_SERIALIZATION_UBJSON_MACROS_H
#define QN_SERIALIZATION_UBJSON_MACROS_H

#include <utility> /* For std::move. */

#include "ubjson.h"

#include <utils/fusion/fusion_serialization.h>

namespace QnUbjsonDetail {
    template<class Output>
    class SerializationVisitor {
    public:
        SerializationVisitor(QnUbjsonWriter<Output> *stream): 
            m_stream(stream) 
        {}

        template<class T, class Access>
        bool operator()(const T &, const Access &, const QnFusion::start_tag &) const {
            using namespace QnFusion;
            m_stream->writeArrayStart();
            return true;
        }

        template<class T, class Access>
        bool operator()(const T &value, const Access &access) const {
            using namespace QnFusion;
            QnUbjson::serialize(invoke(access(getter), value), m_stream);
            return true;
        }

        template<class T, class Access>
        bool operator()(const T &, const Access &, const QnFusion::end_tag &) const {
            using namespace QnFusion;
            m_stream->writeArrayEnd();
            return true;
        }

    private:
        QnUbjsonWriter<Output> *m_stream;
    };

    template<class Input>
    struct DeserializationVisitor {
    public:
        DeserializationVisitor(QnUbjsonReader<Input> *stream):
            m_stream(stream)
        {}

        template<class T, class Access>
        bool operator()(const T &, const Access &, const QnFusion::start_tag &) {
            using namespace QnFusion;

            if(!m_stream->readArrayStart())
                return false;

            return true;
        }

        template<class T, class Access>
        bool operator()(T &target, const Access &access) {
            using namespace QnFusion;

            /* Packet from previous version? Just skip the new fields. */
            if(m_stream->peekMarker() == QnUbjson::ArrayEndMarker)
                return true; 

            return operator()(target, access, access(setter_tag));
        }

        template<class T, class Access>
        bool operator()(const T &, const Access &, const QnFusion::end_tag &) {
            using namespace QnFusion;

            /* Packet from next version? Skip additional values. */
            while(m_stream->peekMarker() != QnUbjson::ArrayEndMarker)
                if(!m_stream->skipValue())
                    return false; 

            return m_stream->readArrayEnd();
        }

    private:
        template<class T, class Access>
        bool operator()(T &target, const Access &access, const QnFusion::member_setter_tag &) {
            using namespace QnFusion;

            return QnUbjson::deserialize(m_stream, &(target.*access(setter)));
        }

        template<class T, class Access, class Member>
        bool operator()(T &target, const Access &access, const QnFusion::typed_function_setter_tag<Member> &) {
            using namespace QnFusion;

            Member member;
            if(!QnUbjson::deserialize(m_stream, &member))
                return false;
            invoke(access(setter), target, std::move(member));
            return true;
        }

    private:
        QnUbjsonReader<Input> *m_stream;
    };

} // namespace QnBinaryDetail


QN_FUSION_REGISTER_SERIALIZATION_VISITOR_TPL((class Output), (QnUbjsonWriter<Output> *), (QnUbjsonDetail::SerializationVisitor<Output>))
QN_FUSION_REGISTER_DESERIALIZATION_VISITOR_TPL((class Input), (QnUbjsonReader<Input> *), (QnUbjsonDetail::DeserializationVisitor<Input>))


#define QN_FUSION_DEFINE_FUNCTIONS_ubjson(TYPE, ... /* PREFIX */)               \
__VA_ARGS__ void serialize(const TYPE &value, QnUbjsonWriter<QByteArray> *stream) { \
    QnFusion::serialize(value, &stream);                                        \
}                                                                               \
                                                                                \
__VA_ARGS__ bool deserialize(QnUbjsonReader<QByteArray> *stream, TYPE *target) { \
    return QnFusion::deserialize(stream, target);                               \
}


#endif // QN_SERIALIZATION_UBJSON_MACROS_H
