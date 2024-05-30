// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtQuick/QQuickImageProvider>

namespace nx::vms::client::desktop {

/**
 * This is a temporary solution to pass Qt icon pixmaps from QnResourceItemCache to QML.
 * Icon paths are "image://<provider id>/<key>/<state>"
 *     <provider id> is an id passed to QQmlEngine::addImageProvider.
 *     <key> is a combination of QnResourceItemCache::Key flags.
 *     <state> is an optional value of ResourceTreeModelAdapter::ItemState type.
 */
class ResourceIconProvider: public QQuickImageProvider
{
public:
    ResourceIconProvider(): QQuickImageProvider(ImageType::Pixmap) {}

    virtual QPixmap requestPixmap(
        const QString& idAndParams, QSize* size, const QSize& requestedSize) override;
};

} // namespace nx::vms::client::desktop
