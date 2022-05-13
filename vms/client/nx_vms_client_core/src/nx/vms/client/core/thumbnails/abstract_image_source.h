// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>
#include <QtCore/QUrl>
#include <QtCore/QUuid>
#include <QtGui/QPixmap>

namespace nx::vms::client::core {

class ThumbnailImageProvider;

/**
 * A base class for image sources for generic QML image provider ThumbnailImageProvider.
 */
class NX_VMS_CLIENT_CORE_API AbstractImageSource
{
public:
    const QString id;

public:
    AbstractImageSource(ThumbnailImageProvider* provider,
        const QString& id = QUuid::createUuid().toString(QUuid::WithoutBraces));

    virtual ~AbstractImageSource();

    /** Image by id. To be implemented by descendants. */
    virtual QImage image(const QString& imageId) const = 0;

    /** Builds an URL to access a specified image from QML code. */
    QUrl makeUrl(const QString& imageId) const;

protected:
    QPointer<ThumbnailImageProvider> m_provider;
};

} // namespace nx::vms::client::core
