#ifndef QN_UBJ_H
#define QN_UBJ_H

#include "ubj_fwd.h"
#include "ubj_reader.h"
#include "ubj_writer.h"

namespace QnUbj {
    template<class T, class Output>
    void serialize(const T &value, QnUbjWriter<Output> *stream) {
        QnSerialization::serialize(value, stream);
    }

    template<class T, class Input>
    bool deserialize(QnUbjReader<Input> *stream, T *target) {
        return QnSerialization::deserialize(stream, target);
    }

#ifndef QN_NO_QT
    template<class T>
    QByteArray serialized(const T &value) {
        QByteArray result;
        QnUbjWriter<QByteArray> stream(&result);
        QnUbj::serialize(value, &stream);
        return result;
    }

    template<class T>
    T deserialized(const QByteArray &value, const T &defaultValue = T(), bool *success = NULL) {
        T target;
        QnUbjReader<QByteArray> stream(&value);
        bool result = QnUbj::deserialize(&stream, &target);
        if (success)
            *success = result;
        return result ? target : defaultValue;
    }
#endif

} // namespace QnUbj


namespace QnUbjDetail {
    template<class Output>
    class SerializationVisitor {
    public:
        SerializationVisitor(QnUbjWriter<Output> *stream): 
            m_stream(stream) 
        {}

        template<class T, class Access>
        bool operator()(const T &, const Access &access, const QnFusion::start_tag &) const {
            using namespace QnFusion;
            m_stream->writeArrayStart(access(member_count));
            return true;
        }

        template<class T, class Access>
        bool operator()(const T &value, const Access &access) const {
            using namespace QnFusion;
            QnUbj::serialize(invoke(access(getter), value), m_stream);
            return true;
        }

        template<class T, class Access>
        bool operator()(const T &, const Access &, const QnFusion::end_tag &) const {
            using namespace QnFusion;
            m_stream->writeArrayEnd();
            return true;
        }

    private:
        QnUbjWriter<Output> *m_stream;
    };

    template<class Input>
    struct DeserializationVisitor {
    public:
        DeserializationVisitor(QnUbjReader<Input> *stream):
            m_stream(stream)
        {}

        template<class T, class Access>
        bool operator()(const T &, const Access &, const QnFusion::start_tag &) {
            using namespace QnFusion;

            if(!m_stream->readArrayStart(&m_count))
                return false;
            if(m_count < 0)
                return false; /* Invalid format, size is expected to be specified. */

            return true;
        }

        template<class T, class Access>
        bool operator()(T &target, const Access &access) {
            using namespace QnFusion;

            m_count--;
            if(m_count < 0)
                return true; /* A packet from the previous version, OK. */

            return operator()(target, access, access(setter_tag));
        }

        template<class T, class Access>
        bool operator()(const T &, const Access &, const QnFusion::end_tag &) {
            using namespace QnFusion;

            if(m_count > 0)
                return false; // TODO: #Elric #UBJ skip

            return m_stream->readArrayEnd();
        }

    private:
        template<class T, class Access>
        bool operator()(T &target, const Access &access, const QnFusion::member_setter_tag &) {
            using namespace QnFusion;

            return QnUbj::deserialize(m_stream, &(target.*access(setter)));
        }

        template<class T, class Access, class Member>
        bool operator()(T &target, const Access &access, const QnFusion::typed_function_setter_tag<Member> &) {
            using namespace QnFusion;

            Member member;
            if(!QnUbj::deserialize(m_stream, &member))
                return false;
            invoke(access(setter), target, std::move(member));
            return true;
        }

    private:
        QnUbjReader<Input> *m_stream;
        int m_count;
    };

} // namespace QnBinaryDetail


QN_FUSION_REGISTER_SERIALIZATION_VISITOR_TPL((class Output), (QnUbjWriter<Output> *), (QnUbjDetail::SerializationVisitor<Output>))
QN_FUSION_REGISTER_DESERIALIZATION_VISITOR_TPL((class Input), (QnUbjReader<Input> *), (QnUbjDetail::DeserializationVisitor<Input>))


#define QN_FUSION_DEFINE_FUNCTIONS_ubj(TYPE, ... /* PREFIX */)                  \
__VA_ARGS__ void serialize(const TYPE &value, QnUbjWriter<QByteArray> *stream) { \
    QnFusion::serialize(value, &stream);                                        \
}                                                                               \
                                                                                \
__VA_ARGS__ bool deserialize(QnUbjReader<QByteArray> *stream, TYPE *target) {   \
    return QnFusion::deserialize(stream, target);                               \
}


#endif // QN_UBJ_H
