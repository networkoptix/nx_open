// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#define _NX_PP_CAT_I(X, Y) X ## Y
#define NX_PP_CAT(X, Y) _NX_PP_CAT_I(X, Y)

#define _NX_PP_STRINGIZE_I(X) #X
#define NX_PP_STRINGIZE(X) _NX_PP_STRINGIZE_I(X)

#define NX_PP_EXPAND(...) __VA_ARGS__
