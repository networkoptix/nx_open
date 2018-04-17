#pragma once

#include <QtWidgets/QLabel>
#include <QtWidgets/QSlider>
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
    void mouseMoveEvent(QMouseEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void leaveEvent(QEvent * event);

private:
    bool m_isClicked = false;
    bool m_isHovered = false;
    QPixmap m_normal;
    QPixmap m_highlighted;
};

} // namespace desktop
} // namespace client
} // namespace nx
