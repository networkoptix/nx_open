#pragma once

#include <core/resource/resource_fwd.h>
#include <ui/graphics/items/generic/styled_tooltip_widget.h>

#include <nx/vms/client/desktop/common/widgets/async_image_widget.h>

class QGraphicsProxyWidget;

namespace nx::vms::client::desktop {

class TextEditLabel;
class ImageProvider;

} // namespace nx::vms::client::desktop

class QnGraphicsToolTipWidget: public QnStyledTooltipWidget
{
    Q_OBJECT

    using base_type = QnStyledTooltipWidget;
    using AsyncImageWidget = nx::vms::client::desktop::AsyncImageWidget;

public:
    QnGraphicsToolTipWidget(QGraphicsItem* parent = nullptr);

    /** Tooltip text */
    QString text() const;

    /**
    * Set tooltip text.
    * \param text                      New text for this tool tip's label.
    * \reimp
    */
    void setText(const QString& text);

    void setThumbnailVisible(bool visible);

    void setImageProvider(nx::vms::client::desktop::ImageProvider* provider);

    void setHighlightRect(const QRectF& relativeRect);

    QSize maxThumbnailSize() const;
    void setMaxThumbnailSize(const QSize& value);

    //reimp
    void pointTo(const QPointF& pos);
    virtual void updateTailPos() override;

    AsyncImageWidget::CropMode cropMode() const;
    void setCropMode(AsyncImageWidget::CropMode value);

signals:
    void thumbnailClicked();

protected:
    virtual bool sceneEventFilter(QGraphicsItem* watched, QEvent* event) override;

private:
    void forceLayoutUpdate();

protected:
    QPointF m_pointTo;

private:
    QGraphicsProxyWidget* m_proxyWidget;
    QWidget* m_embeddedWidget;
    nx::vms::client::desktop::TextEditLabel* m_textLabel;
    nx::vms::client::desktop::AsyncImageWidget* m_previewWidget;
    QSize m_maxThumbnailSize;
};
