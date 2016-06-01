#pragma once

#include <QString>

#include <core/resource/resource_fwd.h>

QString extractHost(const QString &url);
void getResourceDisplayInformation(const QnResourcePtr& resource, QString& name, QString& extInfo);
QString getFullResourceName(const QnResourcePtr& resource, bool showIp);
inline QString getShortResourceName(const QnResourcePtr& resource) { return getFullResourceName(resource, false); }
