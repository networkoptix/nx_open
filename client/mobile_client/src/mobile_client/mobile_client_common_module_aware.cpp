#include "mobile_client_common_module_aware.h"

#include <mobile_client/mobile_client_module.h>

QnMobileClientCommonModuleAware::QnMobileClientCommonModuleAware():
    base_type(qnMobileClientModule->commonModule())
{
}
