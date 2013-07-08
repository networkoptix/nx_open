#include "ui_resource_name.h"

#include <client/client_settings.h>
#include <core/resource/resource_name.h>

QString getResourceName(const QnResourcePtr &resource) {
    return getFullResourceName(resource, qnSettings->isIpShownInTree());
}


