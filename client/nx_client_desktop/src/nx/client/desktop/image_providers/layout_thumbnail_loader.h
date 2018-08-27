#pragma once

#include <QtCore/QScopedPointer>

#include <api/helpers/thumbnail_request_data.h>
#include <client_core/connection_context_aware.h>
#include <core/resource/resource_fwd.h>

#include "image_provider.h"

namespace nx {

namespace core { struct Watermark; }

namespace client {
namespace desktop {

class LayoutThumbnailLoader:
    public ImageProvider,
    public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = ImageProvider;

public:
    explicit LayoutThumbnailLoader(
        const QnLayoutResourcePtr& layout,
        const QSize& maximumSize,
        qint64 msecSinceEpoch,
        QObject* parent = nullptr);

    virtual ~LayoutThumbnailLoader() override;

    virtual QImage image() const override;
    virtual QSize sizeHint() const override;
    virtual Qn::ThumbnailStatus status() const override;

    QnLayoutResourcePtr layout() const;

    void setWatermark(const nx::core::Watermark& watermark);

    QColor itemBackgroundColor() const;
    void setItemBackgroundColor(const QColor& value);
    void setRequestRoundMethod(api::ImageRequest::RoundMethod roundMethod);

    void setResourcePool(const QPointer<QnResourcePool>& pool);

    QColor fontColor() const;
    void setFontColor(const QColor& value);

protected:
    virtual void doLoadAsync() override;

private:
    struct Private;
    QScopedPointer<Private> d;

    QPointer<QnResourcePool> m_resourcePool;
};

} // namespace desktop
} // namespace client
} // namespace nx
