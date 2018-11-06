#pragma once

#include <QtWidgets/QAbstractButton>

namespace nx::vms::client::desktop {

/**
 * Button with two or three icons, for normal, hovered and optionally pressed state.
 */
class HoverButton: public QAbstractButton
{
    Q_OBJECT
    using base_type = QAbstractButton;

public:
    // Use this one if you do NOT want the pressed state.
    HoverButton(const QString& normalPixmap, const QString& hoveredPixmap, QWidget* parent = nullptr);

    // Use this one if you want the pressed state.
    HoverButton(const QString& normalPixmap, const QString& hoveredPixmap,
        const QString& pressedPixmap, QWidget* parent = nullptr);

    virtual QSize sizeHint() const override;

protected:
    virtual void paintEvent(QPaintEvent* event) override;

private:
    QPixmap m_normal;
    QPixmap m_hovered;
    QPixmap m_pressed;
};

} // namespace nx::vms::client::desktop
