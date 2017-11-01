#pragma once

#include <QtGui/QPixmapCache>

namespace nx {
namespace client {
namespace desktop {

class ResourceWidgetPixmapCache: public QObject
{
    Q_OBJECT

public:
    ResourceWidgetPixmapCache(QObject* parent = nullptr);

    QPixmap pixmap(const QString& fileName) const;

private:
    QPixmapCache m_pixmapCache;
};

} // namespace desktop
} // namespace client
} // namespace nx
