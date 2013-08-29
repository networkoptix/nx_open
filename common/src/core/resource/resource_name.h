#ifndef RESOURCE_NAME_H
#define RESOURCE_NAME_H

#include <QString>

#include <core/resource/resource_fwd.h>

QString getFullResourceName(const QnResourcePtr& resource, bool showIp);

#endif // RESOURCE_NAME_H
