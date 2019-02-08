#include "connection_context_aware.h"

#include <client_core/client_core_module.h>

QnConnectionContextAware::QnConnectionContextAware():
    base_type(qnClientCoreModule->commonModule())
{
}
