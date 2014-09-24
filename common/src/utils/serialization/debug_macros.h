#ifndef QN_SERIALIZATION_DEBUG_MACROS_H
#define QN_SERIALIZATION_DEBUG_MACROS_H

#include <utils/fusion/fusion.h>

namespace QnDebugSerialization {
    class SerializationVisitor {
    public:
        SerializationVisitor(QDebug &stream): 
            m_stream(stream) 
        {}

        template<class T, class Access>
        bool operator()(const T &, const Access &access, const QnFusion::start_tag &) {
            using namespace QnFusion;
            m_stream.nospace() << access(c_classname) << " {";
            return true;
        }

        template<class T, class Access>
        bool operator()(const T &value, const Access &access) {
            using namespace QnFusion;
            m_stream.nospace() << access(c_name) << ": " << invoke(access(getter), value) << "; ";
            return true;
        }

        template<class T, class Access>
        bool operator()(const T &, const Access &, const QnFusion::end_tag &) {
            m_stream.nospace() << '}';
            m_stream.space();
            return true;
        }

    private:
        QDebug &m_stream;
    };

} // namespace QnDebugSerialization


/**
 * This macro generates <tt>QDebug</tt> stream functions for
 * the given struct type.
 *
 * \param TYPE                          Struct type to define <tt>QDebug</tt> stream functions for.
 * \param FIELD_SEQ                     Preprocessor sequence of all fields of the
 *                                      given type that are to be streamed.
 * \param PREFIX                        Optional function definition prefix, e.g. <tt>inline</tt>.
 */
#define QN_FUSION_DEFINE_FUNCTIONS_debug(TYPE, ... /* PREFIX */)                \
__VA_ARGS__ QDebug &operator<<(QDebug &stream, const TYPE &value) {             \
    QnFusion::visit_members(value, QnDebugSerialization::SerializationVisitor(stream)); \
    return stream;                                                              \
}


#endif // QN_SERIALIZATION_DEBUG_MACROS_H
