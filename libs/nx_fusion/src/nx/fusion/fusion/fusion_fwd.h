// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#ifndef Q_MOC_RUN
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/tuple/enum.hpp>
#include <boost/preprocessor/cat.hpp>
#endif // Q_MOC_RUN

#include <boost_pp_variadic_seq/variadic_seq_for_each.h>

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

#endif // Q_MOC_RUN
