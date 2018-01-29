#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/graphics/items/generic/styled_tooltip_widget.h>

class QnImageProvider;
class QnTextEditLabel;
class QGraphicsProxyWidget;

namespace nx { namespace client { namespace desktop { class AsyncImageWidget; } } }

class QnGraphicsToolTipWidget: public QnStyledTooltipWidget
{
    Q_OBJECT

    using base_type = QnStyledTooltipWidget;
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

    void setImageProvider(QnImageProvider* provider);

    QSize maxThumbnailSize() const;
    void setMaxThumbnailSize(const QSize& value);

    //reimp
    void pointTo(const QPointF& pos);
    virtual void updateTailPos() override;

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
    QnTextEditLabel* m_textLabel;
    nx::client::desktop::AsyncImageWidget* m_previewWidget;
    QSize m_maxThumbnailSize;
};
