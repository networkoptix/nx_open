// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QStringList>
#include <QtGui/QPixmap>

namespace nx::vms::client::desktop {

struct SoftwareTriggerPixmaps
{
    /** Path to software trigger pixmaps. */
    static QString pixmapsPath();

    /** Default software trigger pixmap, also a fallback pixmap. */
    static QString defaultPixmapName();

    /** Effective pixmap name with fallback to default if specified name is not available. */
    static QString effectivePixmapName(const QString& name);

    /**
     * Effective pixmap path, name and extension.
     * Falls back to default if specified name is not available.
     */
    static QString effectivePixmapPath(const QString& name);

    /** List of pixmap names available to users to choose for software triggers. */
    static const QStringList& pixmapNames();

    /** Checks if specified pixmap name is valid. */
    static bool hasPixmap(const QString& name);

    /** Get a pixmap by it's name with fallback to default if specified is not available. */
    static QPixmap pixmapByName(const QString& name);

    /** Get colorized pixmap with fallback to default if specified is not available. */
    static QPixmap colorizedPixmap(const QString& name, const QColor& color);
};

} // namespace nx::vms::client::desktop
