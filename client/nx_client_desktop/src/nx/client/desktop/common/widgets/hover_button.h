#pragma once

#include <QtWidgets/QPushButton>

namespace nx {
namespace client {
namespace desktop {

/**
 * Button with two icons, for normal state and hovered state.
 * TODO: Proper events to be implemented
 */
class HoverButton : public QAbstractButton
{
    Q_OBJECT;
    using base_type = QAbstractButton;

public:
    HoverButton(const QString& normal, const QString& highligthed, QWidget* parent);

protected:
    QSize sizeHint() const override;
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent * event) override;

private:
    bool m_isClicked = false;
    bool m_isHovered = false;
    QPixmap m_normal;
    QPixmap m_highlighted;
};

} // namespace desktop
} // namespace client
} // namespace nx
