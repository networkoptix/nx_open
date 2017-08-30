#pragma once

#include <QtCore/QScopedPointer>

#include <api/helpers/thumbnail_request_data.h>
#include <client_core/connection_context_aware.h>
#include <core/resource/resource_fwd.h>
#include <utils/image_provider.h>

namespace nx {
namespace client {
namespace desktop {

class LayoutThumbnailLoader:
    public QnImageProvider,
    public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = QnImageProvider;

public:
    explicit LayoutThumbnailLoader(
        const QnLayoutResourcePtr& layout,
        const QSize maximumSize,
        qint64 msecSinceEpoch = QnThumbnailRequestData::kLatestThumbnail,
        QnThumbnailRequestData::ThumbnailFormat format = QnThumbnailRequestData::JpgFormat,
        QObject* parent = nullptr);

    virtual ~LayoutThumbnailLoader() override;

    virtual QImage image() const override;
    virtual QSize sizeHint() const override;
    virtual Qn::ThumbnailStatus status() const override;

    QnLayoutResourcePtr layout() const;

    QColor itemBackgroundColor() const;
    void setItemBackgroundColor(const QColor& value);

    QColor fontColor() const;
    void setFontColor(const QColor& value);

protected:
    virtual void doLoadAsync() override;

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
