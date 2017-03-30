#pragma once

#include <common/common_module_aware.h>

class QnMobileClientCommonModuleAware: public QnCommonModuleAware
{
    using base_type = QnCommonModuleAware;
public:
    QnMobileClientCommonModuleAware();
};

// Will be redefined when client_core module will be redefined
using QnConnectionContextAware = QnMobileClientCommonModuleAware;
