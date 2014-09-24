//#define QT_NO_CAST_FROM_ASCII // TODO: #Elric

#include <common/config.h>
#ifdef __cplusplus
#   include <common/common_globals.h>
#endif


#ifdef _WIN32
#   if _MSC_VER < 1800
#       define noexcept
#   endif
#endif
