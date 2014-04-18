#ifndef QN_MODEL_FUNCTIONS_H
#define QN_MODEL_FUNCTIONS_H

#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/to_list.hpp>
#include <boost/preprocessor/list/for_each.hpp>
#include <boost/preprocessor/cat.hpp>

#include <QtCore/QDataStream>
#include <QtCore/QHash>

#include <utils/serialization/json.h>
#include <utils/math/fuzzy.h>

#include "model_functions_fwd.h"

#ifdef Q_MOC_RUN
/* Qt5 moc chokes on these macros, so we provide dummy definitions. */
#define QN_DEFINE_STRUCT_FUNCTIONS(...)
#define QN_DEFINE_STRUCT_HASH_FUNCTION(...)
#define QN_DEFINE_STRUCT_OPERATOR_EQ(...)
#define QN_DEFINE_STRUCT_DATA_STREAM_FUNCTIONS(...)
#define QN_DEFINE_STRUCT_DEBUG_STREAM_FUNCTIONS(...)
#else // Q_MOC_RUN

namespace QnModelFunctionsDetail {
    template<class T>
    inline bool equals(const T &l, const T &r) { return l == r; }

    inline bool equals(float l, float r) { return qFuzzyEquals(l, r); }
    inline bool equals(double l, double r) { return qFuzzyEquals(l, r); }
} // namespace QnModelFunctionsDetail


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
#define QN_DEFINE_STRUCT_FUNCTIONS(TYPE, FUNCTION_SEQ, FIELD_SEQ, ... /* PREFIX */) \
    BOOST_PP_LIST_FOR_EACH(QN_DEFINE_STRUCT_FUNCTIONS_STEP_I, (TYPE, FIELD_SEQ, ##__VA_ARGS__), BOOST_PP_SEQ_TO_LIST(FUNCTION_SEQ))
/* Note this ^^^. We're converting sequence to a list because this is the only way 
 * to make nested sequence iterations work. The other option would be to use BOOST_PP_FOR directly
 * in all macros, which is just too messy. */

#define QN_DEFINE_STRUCT_FUNCTIONS_STEP_I(R, DATA, FUNCTION)                    \
    BOOST_PP_CAT(QN_DEFINE_STRUCT_FUNCTIONS_STEP_I_, FUNCTION) DATA

#define QN_DEFINE_STRUCT_FUNCTIONS_STEP_I_hash          QN_DEFINE_STRUCT_HASH_FUNCTION
#define QN_DEFINE_STRUCT_FUNCTIONS_STEP_I_datastream    QN_DEFINE_STRUCT_DATA_STREAM_FUNCTIONS
#define QN_DEFINE_STRUCT_FUNCTIONS_STEP_I_eq            QN_DEFINE_STRUCT_OPERATOR_EQ
#define QN_DEFINE_STRUCT_FUNCTIONS_STEP_I_json          QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS
#define QN_DEFINE_STRUCT_FUNCTIONS_STEP_I_debug         QN_DEFINE_STRUCT_DEBUG_STREAM_FUNCTIONS


/**
 * This macro generates <tt>qHash</tt> function for the given struct type.
 * 
 * \param TYPE                          Struct type to define hash function for.
 * \param FIELD_SEQ                     Preprocessor sequence of all fields of the
 *                                      given type that are to be combined in hash 
 *                                      calculation.
 * \param PREFIX                        Optional function definition prefix, e.g. <tt>inline</tt>.
 */
#define QN_DEFINE_STRUCT_HASH_FUNCTION(TYPE, FIELD_SEQ, ... /* PREFIX */)       \
__VA_ARGS__ uint qHash(const TYPE &value, uint seed) {                          \
    return 0 BOOST_PP_SEQ_FOR_EACH(QN_DEFINE_STRUCT_QHASH_STEP_I, ~, FIELD_SEQ); \
}

#define QN_DEFINE_STRUCT_QHASH_STEP_I(R, DATA, FIELD) ^ qHash(value.FIELD, seed)


/**
 * This macro generates <tt>operator==</tt> and <tt>operator!=</tt> functions for
 * the given struct type.
 * 
 * \param TYPE                          Struct type to define comparison operators for.
 * \param FIELD_SEQ                     Preprocessor sequence of all fields of the
 *                                      given type that are to be compared.
 * \param PREFIX                        Optional function definition prefix, e.g. <tt>inline</tt>.
 */
#define QN_DEFINE_STRUCT_OPERATOR_EQ(TYPE, FIELD_SEQ, ... /* PREFIX */)         \
__VA_ARGS__ bool operator==(const TYPE &l, const TYPE &r) {                     \
    using namespace QnModelFunctionsDetail;                                     \
    return true BOOST_PP_SEQ_FOR_EACH(QN_DEFINE_STRUCT_OPERATOR_EQ_STEP_I, ~, FIELD_SEQ); \
}                                                                               \
                                                                                \
__VA_ARGS__ bool operator!=(const TYPE &l, const TYPE &r) {                     \
    return !(l == r);                                                           \
}

#define QN_DEFINE_STRUCT_OPERATOR_EQ_STEP_I(R, DATA, FIELD) && equals(l.FIELD, r.FIELD)


/**
 * This macro generates <tt>QDataStream</tt> (de)serialization functions for
 * the given struct type.
 * 
 * \param TYPE                          Struct type to define (de)serialization functions for.
 * \param FIELD_SEQ                     Preprocessor sequence of all fields of the
 *                                      given type that are to be (de)serialized.
 * \param PREFIX                        Optional function definition prefix, e.g. <tt>inline</tt>.
 */
#define QN_DEFINE_STRUCT_DATA_STREAM_FUNCTIONS(TYPE, FIELD_SEQ, ... /* PREFIX */) \
__VA_ARGS__ QDataStream &operator<<(QDataStream &stream, const TYPE &value) {   \
    return stream BOOST_PP_SEQ_FOR_EACH(QN_DEFINE_STRUCT_DATA_STREAM_STEP_I, <<, FIELD_SEQ); \
}                                                                               \
                                                                                \
__VA_ARGS__ QDataStream &operator>>(QDataStream &stream, TYPE &value) {         \
    return stream BOOST_PP_SEQ_FOR_EACH(QN_DEFINE_STRUCT_DATA_STREAM_STEP_I, >>, FIELD_SEQ); \
}

#define QN_DEFINE_STRUCT_DATA_STREAM_STEP_I(R, OP, FIELD) OP value.FIELD


/**
 * This macro generates <tt>QDebug</tt> stream functions for
 * the given struct type.
 *
 * \param TYPE                          Struct type to define <tt>QDebug</tt> stream functions for.
 * \param FIELD_SEQ                     Preprocessor sequence of all fields of the
 *                                      given type that are to be streamed.
 * \param PREFIX                        Optional function definition prefix, e.g. <tt>inline</tt>.
 */
#define QN_DEFINE_STRUCT_DEBUG_STREAM_FUNCTIONS(TYPE, FIELD_SEQ, ... /* PREFIX */) \
__VA_ARGS__ QDebug &operator<<(QDebug &stream, const TYPE &value) {             \
    stream.nospace() << BOOST_PP_STRINGIZE(TYPE) << " {";                       \
    BOOST_PP_SEQ_FOR_EACH(QN_DEFINE_STRUCT_DEBUG_STREAM_STEP_I, ~, FIELD_SEQ);  \
    stream.nospace() << '}';                                                    \
    return stream.space();                                                      \
}

#define QN_DEFINE_STRUCT_DEBUG_STREAM_STEP_I(R, DATA, FIELD)                    \
    stream.nospace() << BOOST_PP_STRINGIZE(FIELD) << ": " << value.FIELD << "; ";

#endif // Q_MOC_RUN

#endif // QN_MODEL_FUNCTIONS_H
