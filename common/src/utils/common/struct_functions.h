#ifndef QN_STRUCT_FUNCTIONS_H
#define QN_STRUCT_FUNCTIONS_H

#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/to_list.hpp>
#include <boost/preprocessor/list/for_each.hpp>
#include <boost/preprocessor/cat.hpp>

#include <QtCore/QDataStream>
#include <QtCore/QHash>

#include <utils/common/json.h>

/**
 * This macro generates several functions for the given struct type. Tokens for
 * the functions to generate are passed in FUNCTION_SEQ parameter. Accepted
 * tokens are:
 * <ul>
 * <li> <tt>qhash</tt>          --- <tt>qHash</tt> function. </li>
 * <li> <tt>qdatastream</tt>    --- <tt>QDataStream</tt> (de)serialization functions. </li>
 * <li> <tt>eq</tt>             --- <tt>operator==</tt> and <tt>operator!=</tt>. </li>
 * <li> <tt>qjson</tt>          --- json (de)serialization functions. </li>
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
// TODO: #Elric ^^^ HACK

#define QN_DEFINE_STRUCT_FUNCTIONS_STEP_I(R, DATA, FUNCTION)                    \
    BOOST_PP_CAT(QN_DEFINE_STRUCT_FUNCTIONS_STEP_I_, FUNCTION) DATA

#define QN_DEFINE_STRUCT_FUNCTIONS_STEP_I_qhash         QN_DEFINE_STRUCT_QHASH_FUNCTION
#define QN_DEFINE_STRUCT_FUNCTIONS_STEP_I_qdatastream   QN_DEFINE_STRUCT_DATA_STREAM_FUNCTIONS
#define QN_DEFINE_STRUCT_FUNCTIONS_STEP_I_eq            QN_DEFINE_STRUCT_OPERATOR_EQ
#define QN_DEFINE_STRUCT_FUNCTIONS_STEP_I_qjson         QN_DEFINE_STRUCT_SERIALIZATION_FUNCTIONS


/**
 * This macro generates <tt>qHash</tt> function for the given struct type.
 * 
 * \param TYPE                          Struct type to define hash function for.
 * \param FIELD_SEQ                     Preprocessor sequence of all fields of the
 *                                      given type that are to be combined in hash 
 *                                      calculation.
 * \param PREFIX                        Optional function definition prefix, e.g. <tt>inline</tt>.
 */
#define QN_DEFINE_STRUCT_QHASH_FUNCTION(TYPE, FIELD_SEQ, ... /* PREFIX */)      \
__VA_ARGS__ uint qHash(const TYPE &value) {                                     \
    return 0 BOOST_PP_SEQ_FOR_EACH(QN_DEFINE_STRUCT_QHASH_STEP_I, ~, FIELD_SEQ); \
}

#define QN_DEFINE_STRUCT_QHASH_STEP_I(R, DATA, FIELD) ^ qHash(value.FIELD)


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
    return true BOOST_PP_SEQ_FOR_EACH(QN_DEFINE_STRUCT_OPERATOR_EQ_STEP_I, ~, FIELD_SEQ); \
}                                                                               \
                                                                                \
__VA_ARGS__ bool operator!=(const TYPE &l, const TYPE &r) {                     \
    return !(l == r);                                                           \
}

#define QN_DEFINE_STRUCT_OPERATOR_EQ_STEP_I(R, DATA, FIELD) && l.FIELD == r.FIELD


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


#endif // QN_STRUCT_FUNCTIONS_H
