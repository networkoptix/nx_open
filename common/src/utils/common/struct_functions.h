#ifndef QN_STRUCT_FUNCTIONS_H
#define QN_STRUCT_FUNCTIONS_H

#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/cat.hpp>

#include <QtCore/QDataStream>
#include <QtCore/QHash>

#include <utils/common/json.h>

#define QN_DEFINE_STRUCT_FUNCTIONS_STEP_I_qhash         QN_DEFINE_STRUCT_QHASH_FUNCTION
#define QN_DEFINE_STRUCT_FUNCTIONS_STEP_I_qdatastream   QN_DEFINE_STRUCT_DATA_STREAM_FUNCTIONS
#define QN_DEFINE_STRUCT_FUNCTIONS_STEP_I_eq            QN_DEFINE_STRUCT_OPERATOR_EQ
#define QN_DEFINE_STRUCT_FUNCTIONS_STEP_I_qjson         QN_DEFINE_STRUCT_SERIALIZATION_FUNCTIONS

#define QN_DEFINE_STRUCT_FUNCTIONS_STEP_I(R, DATA, FUNCTION)                    \
    BOOST_PP_CAT(QN_DEFINE_STRUCT_FUNCTIONS_STEP_I_, FUNCTION) DATA

// TODO: #Elric doxydocs
#define QN_DEFINE_STRUCT_FUNCTIONS(TYPE, FUNCTION_SEQ, FIELD_SEQ, ... /* PREFIX */) \
    BOOST_PP_SEQ_FOR_EACH_R(128, QN_DEFINE_STRUCT_FUNCTIONS_STEP_I, (TYPE, FIELD_SEQ, ##__VA_ARGS__), FUNCTION_SEQ)
// TODO: #Elric             ^^^ HACK

#define QN_DEFINE_STRUCT_QHASH_STEP_I(R, DATA, FIELD) ^ qHash(value.FIELD)

// TODO: #Elric doxydocs
#define QN_DEFINE_STRUCT_QHASH_FUNCTION(TYPE, FIELD_SEQ, ... /* PREFIX */)      \
__VA_ARGS__ uint qHash(const TYPE &value) {                                     \
    return 0 BOOST_PP_SEQ_FOR_EACH(QN_DEFINE_STRUCT_QHASH_STEP_I, ~, FIELD_SEQ); \
}


#define QN_DEFINE_STRUCT_OPERATOR_EQ_STEP_I(R, DATA, FIELD) && l.FIELD == r.FIELD

// TODO: #Elric doxydocs
#define QN_DEFINE_STRUCT_OPERATOR_EQ(TYPE, FIELD_SEQ, ... /* PREFIX */)         \
__VA_ARGS__ bool operator==(const TYPE &l, const TYPE &r) {                     \
    return true BOOST_PP_SEQ_FOR_EACH(QN_DEFINE_STRUCT_OPERATOR_EQ_STEP_I, ~, FIELD_SEQ); \
}                                                                               \
                                                                                \
__VA_ARGS__ bool operator!=(const TYPE &l, const TYPE &r) {                     \
    return !(l == r);                                                           \
}


#define QN_DEFINE_STRUCT_DATA_STREAM_STEP_I(R, OP, FIELD) OP value.FIELD

// TODO: #Elric doxydocs
#define QN_DEFINE_STRUCT_DATA_STREAM_FUNCTIONS(TYPE, FIELD_SEQ, ... /* PREFIX */) \
__VA_ARGS__ QDataStream &operator<<(QDataStream &stream, const TYPE &value) {   \
    return stream BOOST_PP_SEQ_FOR_EACH(QN_DEFINE_STRUCT_DATA_STREAM_STEP_I, <<, FIELD_SEQ); \
}                                                                               \
                                                                                \
__VA_ARGS__ QDataStream &operator>>(QDataStream &stream, TYPE &value) {         \
    return stream BOOST_PP_SEQ_FOR_EACH(QN_DEFINE_STRUCT_DATA_STREAM_STEP_I, >>, FIELD_SEQ); \
}

#endif // QN_STRUCT_FUNCTIONS_H
