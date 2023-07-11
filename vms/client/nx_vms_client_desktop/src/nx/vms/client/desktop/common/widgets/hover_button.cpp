// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "hover_button.h"

#include <QtCore/QString>
#include <QtGui/QPainter>
#include <QtWidgets/QStyle>
#include <QtWidgets/QWidget>

#include <nx/vms/client/core/skin/skin.h>

namespace nx::vms::client::desktop {

HoverButton::HoverButton(const QIcon& icon, QWidget* parent):
    base_type(parent),
    m_normal(icon.pixmap(QSize(20, 20))),
    m_hovered(icon.pixmap(QSize(20, 20), QnIcon::Active)),
    m_pressed(icon.pixmap(QSize(20, 20), QnIcon::Selected))
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover);
}

HoverButton::HoverButton(const QString& normalPixmap, const QString& hoveredPixmap, QWidget* parent):
    HoverButton(normalPixmap, hoveredPixmap, QString(), parent)
{
}

HoverButton::HoverButton(
    const QString& normalPixmap,
    const QString& hoveredPixmap,
    const QString& pressedPixmap,
    QWidget* parent)
    :
    base_type(parent),
    m_normal(normalPixmap.isEmpty() ? QPixmap() : qnSkin->pixmap(normalPixmap)),
    m_hovered(hoveredPixmap.isEmpty() ? QPixmap() : qnSkin->pixmap(hoveredPixmap)),
    m_pressed(pressedPixmap.isEmpty() ? QPixmap() : qnSkin->pixmap(pressedPixmap))
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover);
}

QSize HoverButton::sizeHint() const
{
    return m_normal.size() / m_normal.devicePixelRatio();
}

void HoverButton::paintEvent(QPaintEvent* /*event*/)
{
    QPixmap pixmap;
    if (isDown())
        pixmap = m_pressed.isNull() ? m_normal : m_pressed;
    else if (underMouse())
        pixmap = m_hovered.isNull() ? m_normal : m_hovered;
    else
        pixmap = m_normal;

    if (pixmap.isNull())
        return;

    const auto pixmapRect = QStyle::alignedRect(
        Qt::LeftToRight,
        Qt::AlignCenter,
        pixmap.size() / pixmap.devicePixelRatio(),
        rect());

    QPainter painter(this);
    painter.drawPixmap(pixmapRect, pixmap);
}

} // namespace nx::vms::client::desktop
