// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QPixmapCache>

namespace nx::vms::client::desktop {

class ResourceWidgetPixmapCache: public QObject
{
    Q_OBJECT

public:
    ResourceWidgetPixmapCache(QObject* parent = nullptr);

    QPixmap pixmap(const QString& fileName) const;

private:
    QPixmapCache m_pixmapCache;
};

} // namespace nx::vms::client::desktop
