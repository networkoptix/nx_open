#ifndef QN_FUSION_FWD_H
#define QN_FUSION_FWD_H

#ifndef Q_MOC_RUN
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/tuple/enum.hpp>
#include <boost/preprocessor/cat.hpp>
#endif // Q_MOC_RUN

#include <utils/preprocessor/variadic_seq_for_each.h>

namespace QnFusion {
    template<class Setter, class T>
    struct setter_category;
} // namespace QnFusion


/**
 * Place this macro inside the class's body to allow access to the class's 
 * private fields from the fusion binding.
 */
#define QN_FUSION_ENABLE_PRIVATE()                                              \
    template<class T>                                                           \
    friend struct QnFusionBinding;

#ifdef Q_MOC_RUN
#define QN_FUSION_DECLARE_FUNCTIONS(...)
#define QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(...)
#else


/**
 * This macro generates several function definitions for the provided type.
 * Tokens for the functions to generate are passed in FUNCTION_SEQ parameter. 
 * See QN_FUSION_DEFINE_FUNCTIONS for a list of tokens. 
 * 
 * \param CLASS                         Type to declare functions for.
 * \param FUNCTION_SEQ                  Preprocessor sequence of functions to define.
 * \param PREFIX                        Optional function definition prefix, e.g. <tt>inline</tt>.
 */
#define QN_FUSION_DECLARE_FUNCTIONS(CLASS, FUNCTION_SEQ, ... /* PREFIX */)       \
    BOOST_PP_SEQ_FOR_EACH(QN_FUSION_DECLARE_FUNCTIONS_STEP_I, (CLASS, ##__VA_ARGS__), FUNCTION_SEQ)

#define QN_FUSION_DECLARE_FUNCTIONS_STEP_I(R, PARAMS, FUNCTION)                  \
    BOOST_PP_CAT(QN_FUSION_DECLARE_FUNCTIONS_, FUNCTION) PARAMS


/**
 * Same as QN_FUSION_DECLARE_FUNCTIONS, but for several types.
 * 
 * \param FUNCTION_SEQ                  Preprocessor sequence of types to define functions for.
 * \param FUNCTION_SEQ                  Preprocessor sequence of functions to define.
 * \param PREFIX                        Optional function definition prefix, e.g. <tt>inline</tt>.
 */
#define QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(CLASS_SEQ, FUNCTION_SEQ, ... /* PREFIX */) \
    BOOST_PP_VARIADIC_SEQ_FOR_EACH(QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES_STEP_I, (FUNCTION_SEQ, ##__VA_ARGS__), CLASS_SEQ)

#define QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES_STEP_I(R, PARAMS, CLASS)           \
    QN_FUSION_DECLARE_FUNCTIONS(BOOST_PP_TUPLE_ENUM(CLASS), BOOST_PP_TUPLE_ENUM(PARAMS))

#endif // Q_MOC_RUN

#endif // QN_FUSION_FWD_H
