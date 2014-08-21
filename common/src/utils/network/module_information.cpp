#include "module_information.h"

#include <utils/common/model_functions.h>
#include <common/common_module.h>


bool QnModuleInformation::isCompatibleToCurrentSystem() const {
    return systemName == qnCommon->localSystemName() && isCompatible(version, qnCommon->engineVersion());
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnModuleInformation, (json), QnModuleInformation_Fields)

