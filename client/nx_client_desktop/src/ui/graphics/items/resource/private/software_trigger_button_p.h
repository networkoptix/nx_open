#pragma once

class HoverFocusProcessor;
class QnStyledTooltipWidget;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace graphics {

class SoftwareTriggerButton;

class SoftwareTriggerButtonPrivate: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_DECLARE_PUBLIC(SoftwareTriggerButton)
    SoftwareTriggerButton* const q_ptr;

public:
    SoftwareTriggerButtonPrivate(SoftwareTriggerButton* main);
    virtual ~SoftwareTriggerButtonPrivate();

    QString toolTip() const;
    void setToolTip(const QString& toolTip);

    Qt::Edge toolTipEdge() const;
    void setToolTipEdge(Qt::Edge edge);

    QSize buttonSize() const;
    void setButtonSize(const QSize& size);

    void setIcon(const QString& name);

    bool prolonged() const;
    void setProlonged(bool value);

    void ensureIcon();

private:
    void updateToolTipPosition();
    void updateToolTipTailEdge();
    void updateToolTipVisibility();

private:
    QString m_iconName;
    QSize m_buttonSize;
    Qt::Edge m_toolTipEdge = Qt::LeftEdge;
    bool m_prolonged = false;

    QnStyledTooltipWidget* const m_toolTip;
    HoverFocusProcessor* const m_toolTipHoverProcessor;
    bool m_iconDirty = false;
};

} // namespace graphics
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
