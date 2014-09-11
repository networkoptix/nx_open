#ifndef IPC_PIPE_NAMES_H
#define IPC_PIPE_NAMES_H

#include <utils/common/app_info.h>

//!Name of named pipe, used by applauncher to post tasks to the queue and by client to use compatibility mode
static const QString launcherPipeName( QnAppInfo::customizationName() + lit("EC4C367A-FEF0-4fa9-B33D-DF5B0C767788" ));

#endif // IPC_PIPE_NAMES_H
