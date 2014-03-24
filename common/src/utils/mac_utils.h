#ifndef MAC_UTILS_H
#define MAC_UTILS_H

#include <QtCore/QString>

QString mac_getMoviesDir();
bool mac_startDetached(const QString &path, const QStringList &arguments);

#endif // MAC_UTILS_H
