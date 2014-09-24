#ifndef QN_SERIALIZATION_DATA_STREAM_MACROS_H
#define QN_SERIALIZATION_DATA_STREAM_MACROS_H

#include <QtCore/QDataStream>

#include <utils/fusion/fusion.h>

namespace QnDataStreamSerialization {

    class SerializationVisitor {
    public:
        SerializationVisitor(QDataStream &stream): 
            m_stream(stream) 
        {}

        template<class T, class Access>
        bool operator()(const T &value, const Access &access) {
            using namespace QnFusion;

            m_stream << invoke(access(getter), value);
            return true;
        }

    private:
        QDataStream &m_stream;
    };

    struct DeserializationVisitor {
    public:
        DeserializationVisitor(QDataStream &stream):
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

            m_stream >> (target.*access(setter));
            return true;
        }

        template<class T, class Access, class Member>
        bool operator()(T &target, const Access &access, const QnFusion::typed_function_setter_tag<Member> &) {
            using namespace QnFusion;

            Member member;
            m_stream >> member;
            invoke(access(setter), target, std::move(member));
            return true;
        }

    private:
        QDataStream &m_stream;
    };

} // namespace QnDataStreamSerialization


/**
 * This macro generates <tt>QDataStream</tt> (de)serialization functions for
 * the given struct type.
 * 
 * \param TYPE                          Struct type to define (de)serialization functions for.
 * \param FIELD_SEQ                     Preprocessor sequence of all fields of the
 *                                      given type that are to be (de)serialized.
 * \param PREFIX                        Optional function definition prefix, e.g. <tt>inline</tt>.
 */
#define QN_FUSION_DEFINE_FUNCTIONS_datastream(TYPE, ... /* PREFIX */)           \
__VA_ARGS__ QDataStream &operator<<(QDataStream &stream, const TYPE &value) {   \
    QnFusion::visit_members(value, QnDataStreamSerialization::SerializationVisitor(stream)); \
    return stream;                                                              \
}                                                                               \
                                                                                \
__VA_ARGS__ QDataStream &operator>>(QDataStream &stream, TYPE &value) {         \
    QnFusion::visit_members(value, QnDataStreamSerialization::DeserializationVisitor(stream)); \
    return stream;                                                              \
}


#endif // QN_SERIALIZATION_DATA_STREAM_MACROS_H
