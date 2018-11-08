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
