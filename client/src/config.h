#ifndef QN_CLIENT_CONFIG_H
#define QN_CLIENT_CONFIG_H

#include <utils/common/config.h>

// -------------------------------------------------------------------------- //
// Derived definitions. Do not change.
// -------------------------------------------------------------------------- //
#ifndef QN_SKIN_PATH
#  ifdef CL_CUSTOMIZATION_PRESET
#    define QN_SKIN_PATH QLatin1String(":/") + QLatin1String(CL_CUSTOMIZATION_PRESET)
#  else
#    define QN_SKIN_PATH QLatin1Char(':')
#  endif
#endif


#endif // QN_CLIENT_CONFIG_H
