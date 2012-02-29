#include "resource_property.h"
#include <utils/common/warnings.h>
#include "user_resource.h"

void QnResourceProperty::test() {
    QnUserResource resource;

    for(const char **name = properties; *name != NULL; name++) {
        QVariant value = resource.property(*name);
        if(!value.isValid())
            qnWarning("QnResource has no property named '%1'.", *name);
    }
}

