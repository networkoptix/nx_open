#pragma once

#include <ui/graphics/items/generic/image_button_widget.h>

class QnStyledTooltipWidget;
class HoverFocusProcessor;

class QnSoftwareTriggerButton: public QnImageButtonWidget
{
    Q_OBJECT
    using base_type = QnImageButtonWidget;

public:
    QnSoftwareTriggerButton(QGraphicsItem* parent = nullptr);
    virtual ~QnSoftwareTriggerButton();

    QString toolTip() const;
    void setToolTip(const QString& toolTip);

    Qt::Edge toolTipEdge() const;
    void setToolTipEdge(Qt::Edge edge);

    QSize buttonSize() const;
    void setButtonSize(const QSize& size);

    /* Set icon by name. Pixmap will be obtained from QnSoftwareTriggerIcons::pixmapByName. */
    void setIcon(const QString& name);

    bool prolonged() const;
    void setProlonged(bool value);

    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;

protected:
    using base_type::setIcon;

private:
    void updateToolTipPosition();
    void updateToolTipTailEdge();
    void updateToolTipVisibility();
    void ensureIcon();

private:
    QnStyledTooltipWidget* const m_toolTip;
    HoverFocusProcessor* const m_toolTipHoverProcessor;
    Qt::Edge m_toolTipEdge;
    QString m_iconName;
    QSize m_buttonSize;
    bool m_prolonged = false;
    bool m_iconDirty = false;
};
