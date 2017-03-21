#pragma once

#include <QtCore/QStringList>
#include <QtGui/QIcon>

struct QnSoftwareTriggerIcons
{
    /** Path to software trigger icons. */
    static QString iconsPath();

    /** Default software trigger icon, also a fallback icon. */
    static QString defaultIconName();

    /** List of icon names available to users to choose for software triggers. */
    static const QStringList& iconNames();

    /** Get a pixmap by it's name. */
    static QPixmap pixmapByName(const QString& name);

    /** Get an icon by it's name. */
    static QIcon iconByName(const QString& name);
};
