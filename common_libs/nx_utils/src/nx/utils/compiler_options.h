#pragma once

// -------------------------------------------------------------------------- //
// Library & Compiler globals. Do not change.
// -------------------------------------------------------------------------- //

/* Define NULL. */
#ifdef __cplusplus
#   undef NULL
#   define NULL nullptr
#
#   ifdef __GNUC__
#       undef __null
#       define __null nullptr
#   endif
#endif

