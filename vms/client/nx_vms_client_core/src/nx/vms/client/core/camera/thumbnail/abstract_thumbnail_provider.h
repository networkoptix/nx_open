#pragma once

#include <QtCore/QUrl>
#include <QtCore/QUuid>
#include <QtGui/QPixmap>

namespace nx::vms::client::core {

class AbstractThumbnailProvider
{
public:
    const QString id;

    AbstractThumbnailProvider(
        const QString& id = QUuid::createUuid().toString(QUuid::WithoutBraces));
    virtual ~AbstractThumbnailProvider();

    virtual QPixmap pixmap(const QString& thumbnailId) const = 0;

    QUrl makeUrl(const QString& thumbnailId) const;
};

} // namespace nx::vms::client::core
