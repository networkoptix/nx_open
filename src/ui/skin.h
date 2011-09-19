#ifndef SKIN_H
#define SKIN_H

#include <QtGui/QIcon>
#include <QtGui/QPixmap>

#ifdef CL_DEFAULT_SKIN_PREFIX
#  define CL_SKIN_PREFIX QLatin1String(CL_DEFAULT_SKIN_PREFIX)
#else
#  define CL_SKIN_PREFIX QLatin1Char(':')
#endif

class Skin
{
public:
    static inline QString path(const QString &name)
    { return CL_SKIN_PREFIX + QLatin1String("/skin/") + name; }

    static inline QIcon icon(const QString &name)
    { return QIcon(path(name)); }

    static inline QPixmap pixmap(const QString &name)
    { return QPixmap(path(name)); }
};

#endif // SKIN_H
