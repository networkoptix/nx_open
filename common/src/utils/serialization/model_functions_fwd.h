#ifndef QN_MODEL_FUNCTIONS_FWD_H
#define QN_MODEL_FUNCTIONS_FWD_H

#ifndef Q_MOC_RUN
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/to_list.hpp>
#include <boost/preprocessor/list/for_each.hpp>
#include <boost/preprocessor/tuple/enum.hpp>
#include <boost/preprocessor/cat.hpp>
#endif

#include "json_fwd.h"
#include "lexical_fwd.h"

class QDataStream;

/**
 * \see QN_DEFINE_STRUCT_FUNCTIONS
 */
#ifndef Q_MOC_RUN

#define QN_DECLARE_FUNCTIONS_FOR_TYPES(TYPE_SEQ, FUNCTION_SEQ, ... /* PREFIX */) \
    BOOST_PP_LIST_FOR_EACH(QN_DECLARE_FUNCTIONS_FOR_TYPES_STEP_I, (FUNCTION_SEQ, ##__VA_ARGS__), BOOST_PP_SEQ_TO_LIST(TYPE_SEQ))

#define QN_DECLARE_FUNCTIONS(TYPE, FUNCTION_SEQ, ... /* PREFIX */)              \
    BOOST_PP_SEQ_FOR_EACH(QN_DECLARE_FUNCTIONS_STEP_I, (TYPE, ##__VA_ARGS__), FUNCTION_SEQ)

#define QN_DECLARE_FUNCTIONS_STEP_I(R, DATA, FUNCTION)                          \
    BOOST_PP_CAT(QN_DECLARE_FUNCTIONS_STEP_I_, FUNCTION) DATA

#define QN_DECLARE_FUNCTIONS_FOR_TYPES_STEP_I(R, DATA, TYPE)                    \
    QN_DECLARE_FUNCTIONS(TYPE, BOOST_PP_TUPLE_ENUM(DATA))

#define QN_DECLARE_FUNCTIONS_STEP_I_hash            QN_DECLARE_HASH_FUNCTION
#define QN_DECLARE_FUNCTIONS_STEP_I_datastream      QN_DECLARE_DATA_STREAM_FUNCTIONS
#define QN_DECLARE_FUNCTIONS_STEP_I_eq              QN_DECLARE_OPERATOR_EQ
#define QN_DECLARE_FUNCTIONS_STEP_I_json            QN_DECLARE_JSON_SERIALIZATION_FUNCTIONS
#define QN_DECLARE_FUNCTIONS_STEP_I_lexical         QN_DECLARE_LEXICAL_SERIALIZATION_FUNCTIONS
#define QN_DECLARE_FUNCTIONS_STEP_I_metatype        QN_DECLARE_METATYPE_FUNCTIONS

#else // Q_MOC_RUN

/* Qt moc chokes on our macro hell, so we make things easier for it. */
#define QN_DECLARE_FUNCTIONS_FOR_TYPES(...)
#define QN_DECLARE_FUNCTIONS(...)

#endif // Q_MOC_RUN

#define QN_DECLARE_HASH_FUNCTION(TYPE, ... /* PREFIX */)                        \
__VA_ARGS__ uint qHash(const TYPE &value, uint seed);

#define QN_DECLARE_DATA_STREAM_FUNCTIONS(TYPE, ... /* PREFIX */)                \
__VA_ARGS__ QDataStream &operator<<(QDataStream &stream, const TYPE &value);    \
__VA_ARGS__ QDataStream &operator>>(QDataStream &stream, TYPE &value);

#define QN_DECLARE_OPERATOR_EQ(TYPE, ... /* PREFIX */)                          \
__VA_ARGS__ bool operator==(const TYPE &l, const TYPE &r);                      \
__VA_ARGS__ bool operator!=(const TYPE &l, const TYPE &r);

#define QN_DECLARE_METATYPE_FUNCTIONS(TYPE, ... /* PREFIX */)                   \
Q_DECLARE_METATYPE(TYPE)


#endif // QN_MODEL_FUNCTIONS_FWD_H
