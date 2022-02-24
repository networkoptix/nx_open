// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef MAC_UTILS_H
#define MAC_UTILS_H

#include <QtCore/QString>

NX_VMS_COMMON_API QString mac_getMoviesDir();
NX_VMS_COMMON_API bool mac_startDetached(const QString &path, const QStringList &arguments);
NX_VMS_COMMON_API void mac_openInFinder(const QString &path);

#endif // MAC_UTILS_H
