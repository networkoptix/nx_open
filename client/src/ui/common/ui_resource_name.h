#ifndef RESOURCE_NAME_UI_H
#define RESOURCE_NAME_UI_H

#include <core/resource/resource_fwd.h>

// TODO: #Elric rename, ui_ prefix is for UI generated files.

QString getResourceName(const QnResourcePtr& resource);

QString generateUniqueLayoutName(const QnUserResourcePtr &user, const QString &defaultName, const QString &nameTemplate);

#endif // RESOURCE_NAME_UI_H
