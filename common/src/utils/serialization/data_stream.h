#ifndef QN_SERIALIZATION_DATA_STREAM_H
#define QN_SERIALIZATION_DATA_STREAM_H

#include <QtCore/QDataStream>

namespace QnDataStreamDetail {
    class SerializationVisitor {
    public:
        SerializationVisitor(QDataStream &stream): 
            m_stream(stream) 
        {}

        template<class T, class Adaptor>
        bool operator()(const T &value, const Adaptor &adaptor) {
            using namespace QnFusion;

            m_stream << invoke(adaptor.get(getter));
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

        template<class T, class Adaptor>
        bool operator()(T &target, const Adaptor &adaptor) {
            using namespace QnFusion;

            return operator()(target, adaptor(setter), adaptor);
        }

    private:
        template<class T, class Setter, class Adaptor>
        bool operator()(T &target, const Setter &setter, const Adaptor &adaptor) {
            unused(adaptor);
            using namespace QnFusion;

            typedef typename boost::remove_const<boost::remove_reference<decltype(invoke(adaptor(getter), target))>::type>::type member_type;

            member_type member;
            m_stream >> member;
            invoke(setter, target, std::move(member));
            return true;
        }

        template<class T, class MemberType, class Adaptor>
        bool operator()(T &target, MemberType T::*setter, const Adaptor &adaptor, typename boost::disable_if<boost::is_function<MemberType> >::type * = NULL) {
            unused(adaptor);
            using namespace QnFusion;

            m_stream >> (target.*setter);
            return true;
        }

    private:
        QDataStream &m_stream;
    };

} // namespace QJsonDetail

#endif // QN_SERIALIZATION_DATA_STREAM_H
