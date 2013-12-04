#ifndef RESOURCE_NAME_UI_H
#define RESOURCE_NAME_UI_H

#include <core/resource/resource_fwd.h>

QString getResourceName(const QnResourcePtr& resource);

QString generateUniqueLayoutName(const QnUserResourcePtr &user, const QString &baseName);

#endif // RESOURCE_NAME_UI_H
