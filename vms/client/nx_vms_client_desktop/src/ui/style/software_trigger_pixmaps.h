#pragma once

#include <QtCore/QStringList>
#include <QtGui/QIcon>

struct QnSoftwareTriggerPixmaps
{
    /** Path to software trigger pixmaps. */
    static QString pixmapsPath();

    /** Default software trigger pixmap, also a fallback pixmap. */
    static QString defaultPixmapName();

    /** Effective pixmap name with fallback to default if specified name is not available. */
    static QString effectivePixmapName(const QString& name);

    /** List of pixmap names available to users to choose for software triggers. */
    static const QStringList& pixmapNames();

    /** Checks if specified pixmap name is valid. */
    static bool hasPixmap(const QString& name);

    /** Get a pixmap by it's name with fallback to default if specified is not available. */
    static QPixmap pixmapByName(const QString& name);

    /** Get colorized pixmap with fallback to default if specified is not available. */
    static QPixmap colorizedPixmap(const QString& name, const QColor& color);
};
