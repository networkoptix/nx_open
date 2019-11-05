#pragma once

#include <QtCore/QHash>
#include <QtQuick/QQuickImageProvider>

#include <nx/utils/singleton.h>

namespace nx::vms::client::core {

class AbstractThumbnailProvider;

class ThumbnailImageProvider:
    public QObject,
    public QQuickImageProvider,
    public Singleton<ThumbnailImageProvider>
{
    Q_OBJECT //< Just for instance storage.

public:
    static const QString id;

    ThumbnailImageProvider();

    virtual QPixmap requestPixmap(
        const QString& id, QSize* size, const QSize& requestedSize) override;

    void addProvider(const QString& accessorId, AbstractThumbnailProvider* accessor);
    void removeProvider(const QString& accessorId);

private:
    QHash<QString, AbstractThumbnailProvider*> m_accessorById;
};

} // namespace nx::vms::client::core
