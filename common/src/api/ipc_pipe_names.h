#ifndef IPC_PIPE_NAMES_H
#define IPC_PIPE_NAMES_H

#include "version.h"

//!Name of named pipe, used by applauncher to post tasks to the queue and by client to use compatibility mode
static const QString launcherPipeName( QLatin1String(QN_CUSTOMIZATION_NAME)+QLatin1String("EC4C367A-FEF0-4fa9-B33D-DF5B0C767788") );

#endif // IPC_PIPE_NAMES_H
