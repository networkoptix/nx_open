#pragma once

#include <QtWidgets/QPushButton>

namespace nx {
namespace client {
namespace desktop {

/**
 * Button with two or three icons, for normal, hovered and optionally pressed state.
 */
class HoverButton: public QAbstractButton
{
    Q_OBJECT
    using base_type = QAbstractButton;

public:
    // Use this one if you do NOT want the pressed state.
    HoverButton(const QString& normal, const QString& highlighted, QWidget* parent);
    // Use this one if you want the pressed state.
    HoverButton(const QString& normal, const QString& highlighted,
        const QString& pressed, QWidget* parent);

protected:
    virtual QSize sizeHint() const override;
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void leaveEvent(QEvent* event) override;

private:
    bool m_isHovered = false;
    bool m_hasPressedState = false;
    QPixmap m_normal;
    QPixmap m_highlighted;
    QPixmap m_pressed;
};

} // namespace desktop
} // namespace client
} // namespace nx
