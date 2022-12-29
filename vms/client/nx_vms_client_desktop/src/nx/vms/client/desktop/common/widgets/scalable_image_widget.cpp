// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "scalable_image_widget.h"

#include <QtGui/QPainter>
#include <QtGui/QtEvents>

#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/style/skin.h>

namespace nx::vms::client::desktop {

ScalableImageWidget::ScalableImageWidget(const QString& pixmapPath, QWidget* parent):
    base_type(parent),
    m_pixmapPath(pixmapPath)
{
    m_pixmap = qnSkin->pixmap(m_pixmapPath);
    m_aspectRatio = core::Geometry::aspectRatio(m_pixmap.size());
}

QSize ScalableImageWidget::sizeHint() const
{
    return m_pixmap.size();
}

void ScalableImageWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    const int x = (width() - m_pixmap.width() / m_pixmap.devicePixelRatio()) / 2;
    const int y = (height() - m_pixmap.height() / m_pixmap.devicePixelRatio()) / 2;
    painter.drawPixmap(x, y, m_pixmap);
}

void ScalableImageWidget::resizeEvent(QResizeEvent* event)
{
    if (!m_pixmap.isNull())
    {
        QSizeF size = core::Geometry::scaled(m_aspectRatio, event->size());
        m_pixmap = qnSkin->pixmap(m_pixmapPath, true, size.toSize());
        update();
    }
    base_type::resizeEvent(event);
}

} // namespace nx::vms::client::desktop
