#ifndef QN_FUSION_AUTOMATION_H
#define QN_FUSION_AUTOMATION_H

#include <utils/common/uuid.h>
#include <utils/fusion/fusion_adaptor.h>
#include <utils/serialization/binary_functions.h>
#include <utils/serialization/data_stream_macros.h>
#include <utils/serialization/debug_macros.h>
#include <utils/serialization/csv_functions.h>
#include <utils/serialization/json_functions.h>
#include <utils/serialization/lexical_functions.h>
#include <utils/serialization/sql_functions.h>
#include <utils/serialization/ubjson_functions.h>
#include <utils/serialization/xml_functions.h>
#include <utils/math/fuzzy.h>

namespace QnHashAutomation {
    struct Visitor {
    public:
        Visitor(uint seed): m_result(seed) {}

        template<class T, class Access>
        bool operator()(const T &value, const Access &access) {
            using namespace QnFusion;

            m_result ^= qHash(invoke(access(getter), value));
            return true;
        }

        uint result() const {
            return m_result;
        }

    private:
        uint m_result;
    };

} // namespace QnHashAutomation

namespace QnEqualityAutomation {
    template<class T>
    inline bool equals(const T &l, const T &r) { return l == r; }

    inline bool equals(float l, float r) { return qFuzzyEquals(l, r); }
    inline bool equals(double l, double r) { return qFuzzyEquals(l, r); }

    template<class T>
    struct Visitor {
    public:
        Visitor(const T &l, const T &r): m_l(l), m_r(r) {}

        template<class Access>
        bool operator()(const T &, const Access &access) {
            using namespace QnFusion;

            return equals(invoke(access(getter), m_l), invoke(access(getter), m_r));
        }

    private:
        const T &m_l, &m_r;
    };
} // namespace QnEqualityAutomation


/**
 * This macro generates <tt>qHash</tt> function for the given fusion-adapted type.
 * 
 * \param TYPE                          Fusion-adapted type to define hash function for.
 * \param PREFIX                        Optional function definition prefix, e.g. <tt>inline</tt>.
 */
#define QN_FUSION_DEFINE_FUNCTIONS_hash(TYPE, ... /* PREFIX */)                 \
__VA_ARGS__ uint qHash(const TYPE &value, uint seed) {                          \
    QnHashAutomation::Visitor visitor(seed);                                    \
    QnFusion::visit_members(value, visitor);                                    \
    return visitor.result();                                                    \
}


/**
 * This macro generates <tt>operator==</tt> and <tt>operator!=</tt> functions for
 * the given fusion-adapted type.
 * 
 * \param TYPE                          Fusion-adapted type to define comparison operators for.
 * \param PREFIX                        Optional function definition prefix, e.g. <tt>inline</tt>.
 */
#define QN_FUSION_DEFINE_FUNCTIONS_eq(TYPE, ... /* PREFIX */)                   \
__VA_ARGS__ bool operator==(const TYPE &l, const TYPE &r) {                     \
    QnEqualityAutomation::Visitor<TYPE> visitor(l, r);                          \
    return QnFusion::visit_members(l, visitor);                                 \
}                                                                               \
                                                                                \
__VA_ARGS__ bool operator!=(const TYPE &l, const TYPE &r) {                     \
    return !(l == r);                                                           \
}



#endif // QN_FUSION_AUTOMATION_H



