#pragma once

#include <common/common_module_aware.h>

class QnConnectionContextAware: public QnCommonModuleAware
{
    using base_type = QnCommonModuleAware;
public:
    QnConnectionContextAware();
};
