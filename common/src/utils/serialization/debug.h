#ifndef QN_SERIALIZATION_DEBUG_H
#define QN_SERIALIZATION_DEBUG_H

#include <utils/fusion/fusion.h>

namespace QnDebugSerialization {
    class SerializationVisitor {
    public:
        SerializationVisitor(QDebug &stream): 
            m_stream(stream) 
        {}

        bool operator(const QnFusion::start_tag &) {
            m_stream.nospace() << /* TYPENAME */ " {"; // TODO: #Elric
            return true;
        }

        template<class T, class Access>
        bool operator()(const T &value, const Access &access) {
            using namespace QnFusion;

                                            // TODO: #Elric
            stream.nospace() << access(name).toLatin1().data()  << ": " << invoke(access(getter), value) << "; ";
            return true;
        }

        bool operator(const QnFusion::end_tag &) {
            stream.nospace() << '}';
            stream.space();
            return true;
        }

    private:
        QDebug &m_stream;
    };

} // namespace QnDebugSerialization


#define QN_FUSION_DEFINE_FUNCTIONS_debug(TYPE, ... /* PREFIX */)                \
__VA_ARGS__ QDebug &operator<<(QDebug &stream, const TYPE &value) {             \
    QnFusion::visit_members(value, QnDebugSerialization::SerializationVisitor(stream)); \
    return stream;                                                              \
}


#endif // QN_SERIALIZATION_DEBUG_H
