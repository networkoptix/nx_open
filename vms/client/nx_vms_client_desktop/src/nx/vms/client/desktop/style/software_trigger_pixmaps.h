// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QStringList>
#include <QtGui/QPixmap>

namespace nx::vms::client::desktop {

struct SoftwareTriggerPixmaps
{
    using MapT = QVector<QPair<QString, QPixmap>>;

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

    static const MapT pixmaps();

    /** Checks if specified pixmap name is valid. */
    static bool hasPixmap(const QString& name);

    /** Get a pixmap by it's name with fallback to default if specified is not available. */
    static QPixmap pixmapByName(const QString& name);
};

} // namespace nx::vms::client::desktop
