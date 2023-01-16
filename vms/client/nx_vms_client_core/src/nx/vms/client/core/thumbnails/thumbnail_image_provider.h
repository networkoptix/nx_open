// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtQuick/QQuickImageProvider>

namespace nx::vms::client::core {

class AbstractImageSource;

/**
 * Generic QML image provider with two-level underlying hierarchy: image sources and images.
 */
class NX_VMS_CLIENT_CORE_API ThumbnailImageProvider: public QQuickImageProvider
{
    Q_OBJECT

public:
    static const QString id;

public:
    ThumbnailImageProvider();
    virtual ~ThumbnailImageProvider() override;

    static ThumbnailImageProvider* instance();

    virtual QImage requestImage(
        const QString& id, QSize* size, const QSize& requestedSize) override;

    void addSource(AbstractImageSource* source);
    void removeSource(AbstractImageSource* source);

private:
    QHash<QString, AbstractImageSource*> m_sourceById;
};

} // namespace nx::vms::client::core
