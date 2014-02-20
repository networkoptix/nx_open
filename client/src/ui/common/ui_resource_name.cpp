#include "ui_resource_name.h"

#include <QtCore/QRegExp>

#include <client/client_settings.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/resource_name.h>

#include <utils/common/string.h>

QString getResourceName(const QnResourcePtr &resource) {
    return getFullResourceName(resource, qnSettings->isIpShownInTree());
}

QString generateUniqueLayoutName(const QnUserResourcePtr &user, const QString &baseName) {
    QStringList usedNames;
    QnId parentId = user ? user->getId() : QnId();
    foreach(const QnLayoutResourcePtr &resource, qnResPool->getResourcesWithParentId(parentId).filtered<QnLayoutResource>())
        usedNames.push_back(resource->getName().toLower());
    return generateUniqueString(usedNames, baseName, L' ');
}
