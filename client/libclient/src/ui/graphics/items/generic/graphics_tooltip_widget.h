#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/graphics/items/generic/styled_tooltip_widget.h>

class QnImageProvider;
class QnTextEditLabel;
class QnResourcePreviewWidget;

class QnGraphicsToolTipWidget: public QnStyledTooltipWidget
{
    Q_OBJECT

    using base_type = QnStyledTooltipWidget;
public:
    QnGraphicsToolTipWidget(QGraphicsItem* parent = nullptr);

    /**
    * Set tooltip text.
    * \param text                      New text for this tool tip's label.
    * \reimp
    */
    void setText(const QString& text);

    void setThumbnailVisible(bool visible);

    void setImageProvider(QnImageProvider* provider);

    void setResource(const QnResourcePtr& resource);
    const QnResourcePtr& resource() const;

    //reimp
    void pointTo(const QPointF& pos);
    virtual void updateTailPos() override;

signals:
    void thumbnailClicked();

protected:
    virtual bool sceneEventFilter(QGraphicsItem* watched, QEvent* event) override;

private:
    void forceLayoutUpdate();

private:
    QGraphicsProxyWidget* m_proxyWidget;
    QWidget* m_embeddedWidget;
    QnTextEditLabel* m_textLabel;
    QnResourcePreviewWidget* m_previewWidget;
    QPointF m_pointTo;
};
