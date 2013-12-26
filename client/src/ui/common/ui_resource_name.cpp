#include "ui_resource_name.h"

#include <QtCore/QRegExp>

#include <client/client_settings.h>
#include <core/resource_managment/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/resource_name.h>

QString getResourceName(const QnResourcePtr &resource) {
    return getFullResourceName(resource, qnSettings->isIpShownInTree());
}

QString generateUniqueName(const QStringList &usedNames, const QString &baseName) {
    if (!usedNames.contains(baseName.toLower()))
        return baseName;

    const QString nonZeroName = baseName + QString(QLatin1String(" %1"));
    QRegExp pattern = QRegExp(baseName.toLower() + QLatin1String(" ?([0-9]+)?"));

    /* Prepare new name. */
    int number = 0;
    foreach(const QString &name, usedNames) {
        if(!pattern.exactMatch(name))
            continue;

        number = qMax(number, pattern.cap(1).toInt());
    }
    number++;

    return nonZeroName.arg(number);
}

QString generateUniqueLayoutName(const QnUserResourcePtr &user, const QString &baseName) {
    QStringList usedNames;
    QnId parentId = user ? user->getId() : QnId();
    foreach(const QnLayoutResourcePtr &resource, qnResPool->getResourcesWithParentId(parentId).filtered<QnLayoutResource>())
        usedNames.push_back(resource->getName().toLower());
    return generateUniqueName(usedNames, baseName);
}
