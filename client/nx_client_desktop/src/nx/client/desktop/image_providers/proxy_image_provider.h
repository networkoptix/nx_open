#pragma once

#include <QtCore/QScopedPointer>

#include "image_provider.h"


class QnDisconnectHelper;

namespace nx {
namespace client {
namespace desktop {

class AbstractImageProcessor;

class ProxyImageProvider: public QnImageProvider
{
    Q_OBJECT
    using base_type = QnImageProvider;

public:
    explicit ProxyImageProvider(QObject* parent = nullptr);
    explicit ProxyImageProvider(QnImageProvider* sourceProvider, QObject* parent = nullptr);
    virtual ~ProxyImageProvider() override;

    virtual QImage image() const override;
    virtual QSize sizeHint() const override;
    virtual Qn::ThumbnailStatus status() const override;

    QnImageProvider* sourceProvider() const;
    void setSourceProvider(QnImageProvider* sourceProvider); //< Does not take ownership.

    AbstractImageProcessor* imageProcessor() const;
    void setImageProcessor(AbstractImageProcessor* imageProcessor); //< Does not take ownership.

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
    QnImageProvider* m_sourceProvider = nullptr;
    QScopedPointer<QnDisconnectHelper> m_sourceProviderConnections;

    AbstractImageProcessor* m_imageProcessor = nullptr;
    QScopedPointer<QnDisconnectHelper> m_imageProcessorConnections;

    QImage m_image;
    Qn::ThumbnailStatus m_status = Qn::ThumbnailStatus::Invalid;
    QSize m_sizeHint;
};

} // namespace desktop
} // namespace client
} // namespace nx
