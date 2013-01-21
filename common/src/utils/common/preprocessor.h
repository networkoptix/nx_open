#ifndef QN_PREPROCESSOR_H
#define QN_PREPROCESSOR_H

#include "config.h"

#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/cat.hpp>


// -------------------------------------------------------------------------- //
// Shortcuts
// -------------------------------------------------------------------------- //
#define QN_CAT BOOST_PP_CAT
#define QN_STRINGIZE BOOST_PP_STRINGIZE


#endif // QN_PREPROCESSOR_H
