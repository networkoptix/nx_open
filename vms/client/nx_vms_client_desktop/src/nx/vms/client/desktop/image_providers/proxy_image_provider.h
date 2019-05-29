#pragma once

#include <QtCore/QScopedPointer>

#include "image_provider.h"

#include <client/client_globals.h>
#include <nx/utils/scoped_connections.h>

namespace nx::vms::client::desktop {

class AbstractImageProcessor;

/**
 * This image provider takes input from another ImageProvider
 * and calls AbstractImageProcessor to produce output image
 */
class ProxyImageProvider: public ImageProvider
{
    Q_OBJECT
    using base_type = ImageProvider;

public:
    explicit ProxyImageProvider(QObject* parent = nullptr);
    explicit ProxyImageProvider(ImageProvider* sourceProvider, QObject* parent = nullptr);
    virtual ~ProxyImageProvider() override;

    virtual QImage image() const override;
    virtual QSize sizeHint() const override;
    virtual Qn::ThumbnailStatus status() const override;

    ImageProvider* sourceProvider() const;
    void setSourceProvider(ImageProvider* sourceProvider); //< Does not take ownership.

    AbstractImageProcessor* imageProcessor() const;
    void setImageProcessor(AbstractImageProcessor* imageProcessor); //< Does not take ownership.

    virtual bool tryLoad() override;

protected:
    virtual void doLoadAsync() override;

private:
    void setStatus(Qn::ThumbnailStatus status);
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
    Qn::ThumbnailStatus m_status = Qn::ThumbnailStatus::Invalid;
    QSize m_sizeHint;
};

} // namespace nx::vms::client::desktop
