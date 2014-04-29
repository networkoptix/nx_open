#ifndef QN_FUSION_AUTOMATION_H
#define QN_FUSION_AUTOMATION_H

#include <utils/fusion/fusion_adaptor.h>
#include <utils/serialization/json_functions.h>
#include <utils/serialization/lexical_functions.h>
#include <utils/serialization/binary_functions.h>
#include <utils/serialization/sql_functions.h>
#include <utils/serialization/debug.h>
#include <utils/serialization/data_stream.h>
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

// TODO: DOCZ

/**
 * This macro generates <tt>qHash</tt> function for the given struct type.
 * 
 * \param TYPE                          Struct type to define hash function for.
 * \param FIELD_SEQ                     Preprocessor sequence of all fields of the
 *                                      given type that are to be combined in hash 
 *                                      calculation.
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
 * the given struct type.
 * 
 * \param TYPE                          Struct type to define comparison operators for.
 * \param FIELD_SEQ                     Preprocessor sequence of all fields of the
 *                                      given type that are to be compared.
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


// TODO
/**
 * This macro generates several functions for the given struct type. Tokens for
 * the functions to generate are passed in FUNCTION_SEQ parameter. Accepted
 * tokens are:
 * <ul>
 * <li> <tt>hash</tt>           --- <tt>qHash</tt> function. </li>
 * <li> <tt>datastream</tt>     --- <tt>QDataStream</tt> (de)serialization functions. </li>
 * <li> <tt>eq</tt>             --- <tt>operator==</tt> and <tt>operator!=</tt>. </li>
 * <li> <tt>json</tt>           --- json (de)serialization functions. </li>
 * <li> <tt>debug</tt>          --- <tt>QDebug</tt> streaming functions. </li>
 * </ul>
 * 
 * \param TYPE                          Struct type to define functions for.
 * \param FUNCTION_SEQ                  Preprocessor sequence of functions to define.
 * \param FIELD_SEQ                     Preprocessor sequence of all fields of the
 *                                      given type that are to be used in generated functions.
 * \param PREFIX                        Optional function definition prefix, e.g. <tt>inline</tt>.
 */


#endif // QN_FUSION_AUTOMATION_H



