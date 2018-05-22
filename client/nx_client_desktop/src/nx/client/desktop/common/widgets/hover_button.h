#pragma once

#include <QtWidgets/QPushButton>

namespace nx {
namespace client {
namespace desktop {

/**
 * Button with two icons, for normal state and hovered state.
 */
class HoverButton: public QAbstractButton
{
    Q_OBJECT
    using base_type = QAbstractButton;

public:
    HoverButton(const QString& normal, const QString& highligthed, QWidget* parent);

protected:
    virtual QSize sizeHint() const override;
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void leaveEvent(QEvent* event) override;

private:
    bool m_isHovered = false;
    QPixmap m_normal;
    QPixmap m_highlighted;
};

} // namespace desktop
} // namespace client
} // namespace nx
