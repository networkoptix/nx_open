// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include <client/client_globals.h>
#include <nx/utils/scoped_connections.h>

#include <nx/vms/client/core/image_providers/image_provider.h>

namespace nx::vms::client::desktop {

class AbstractImageProcessor;

/**
 * This image provider takes input from another ImageProvider
 * and calls AbstractImageProcessor to produce output image
 */
class ProxyImageProvider: public core::ImageProvider
{
    Q_OBJECT
    using base_type = core::ImageProvider;

public:
    explicit ProxyImageProvider(QObject* parent = nullptr);
    explicit ProxyImageProvider(core::ImageProvider* sourceProvider, QObject* parent = nullptr);
    virtual ~ProxyImageProvider() override;

    virtual QImage image() const override;
    virtual QSize sizeHint() const override;
    virtual core::ThumbnailStatus status() const override;

    core::ImageProvider* sourceProvider() const;
    void setSourceProvider(core::ImageProvider* sourceProvider); //< Does not take ownership.

    AbstractImageProcessor* imageProcessor() const;
    void setImageProcessor(AbstractImageProcessor* imageProcessor); //< Does not take ownership.

    virtual bool tryLoad() override;

protected:
    virtual void doLoadAsync() override;

private:
    void setStatus(core::ThumbnailStatus status);
    void setSizeHint(const QSize& sizeHint);
    void setImage(const QImage& image);
    void setSourceSizeHint(const QSize& sourceSizeHint);
    void setSourceImage(const QImage& sourceImage);
    void updateFromSource();

private:
    ImageProvider* m_sourceProvider = nullptr;
    nx::utils::ScopedConnections m_sourceProviderConnections;

    AbstractImageProcessor* m_imageProcessor = nullptr;
    nx::utils::ScopedConnections m_imageProcessorConnections;

    QImage m_image;
    core::ThumbnailStatus m_status = core::ThumbnailStatus::Invalid;
    QSize m_sizeHint;
};

} // namespace nx::vms::client::desktop
