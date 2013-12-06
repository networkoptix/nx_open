#ifndef QN_MODEL_FUNCTIONS_FWD_H
#define QN_MODEL_FUNCTIONS_FWD_H

#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/cat.hpp>

#include "json_fwd.h"
#include "lexical_fwd.h"

class QDataStream;

/**
 * \see QN_DEFINE_STRUCT_FUNCTIONS
 */
#define QN_DECLARE_FUNCTIONS(TYPE, FUNCTION_SEQ, ... /* PREFIX */)              \
    BOOST_PP_SEQ_FOR_EACH(QN_DECLARE_FUNCTIONS_STEP_I, (TYPE, ##__VA_ARGS__), FUNCTION_SEQ)

#define QN_DECLARE_FUNCTIONS_STEP_I(R, DATA, FUNCTION)                          \
    BOOST_PP_CAT(QN_DECLARE_FUNCTIONS_STEP_I_, FUNCTION) DATA

#define QN_DECLARE_FUNCTIONS_STEP_I_hash            QN_DECLARE_HASH_FUNCTION
#define QN_DECLARE_FUNCTIONS_STEP_I_datastream      QN_DECLARE_DATA_STREAM_FUNCTIONS
#define QN_DECLARE_FUNCTIONS_STEP_I_eq              QN_DECLARE_OPERATOR_EQ
#define QN_DECLARE_FUNCTIONS_STEP_I_json            QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS
#define QN_DECLARE_FUNCTIONS_STEP_I_lexical         QN_DECLARE_LEXICAL_SERIALIZATION_FUNCTIONS


#define QN_DECLARE_HASH_FUNCTION(TYPE, ... /* PREFIX */)                        \
__VA_ARGS__ uint qHash(const TYPE &value);

#define QN_DECLARE_DATA_STREAM_FUNCTIONS(TYPE, ... /* PREFIX */)                \
__VA_ARGS__ QDataStream &operator<<(QDataStream &stream, const TYPE &value);    \
__VA_ARGS__ QDataStream &operator>>(QDataStream &stream, TYPE &value);

#define QN_DECLARE_OPERATOR_EQ(TYPE, ... /* PREFIX */)                          \
__VA_ARGS__ bool operator==(const TYPE &l, const TYPE &r);                      \
__VA_ARGS__ bool operator!=(const TYPE &l, const TYPE &r);


#endif // QN_MODEL_FUNCTIONS_FWD_H
