// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

// TODO: Remove and replace by NX_FMT or just QString if performance is not required.

#ifdef __cplusplus
/**
 * \def lit
 * Helper macro to mark strings that are not to be translated.
 */
#define QN_USE_QT_STRING_LITERALS
#ifdef QN_USE_QT_STRING_LITERALS
#   define lit(s) QStringLiteral(s)
#else
#   define lit(s) QLatin1String(s)
#endif

#endif
