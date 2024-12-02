// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ptz_overlay_widget.h"

#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/local_settings.h>

using namespace nx::vms::client::desktop;

const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kLight1Theme = {
    {QIcon::Normal, {.primary = "light1", .secondary = "dark7"}}};

NX_DECLARE_COLORIZED_ICON(kZoomInIcon, "32x32/Solid/ptz_zoom_in.svg", kLight1Theme)
NX_DECLARE_COLORIZED_ICON(kZoomOutIcon, "32x32/Solid/ptz_zoom_out.svg", kLight1Theme)
NX_DECLARE_COLORIZED_ICON(kFocusAutoIcon, "32x32/Solid/focus_auto.svg", kLight1Theme)

PtzOverlayWidget::PtzOverlayWidget(QGraphicsItem* parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    m_markersMode(Qt::Horizontal | Qt::Vertical),
    m_pen(QPen(nx::vms::client::core::colorTheme()->color("ptz.main", 192), 0.0))
{
    setAcceptedMouseButtons(Qt::NoButton);

    QPainterPath upRoundPath;
    upRoundPath.moveTo(0, 1);
    upRoundPath.lineTo(0, 0.5);
    upRoundPath.arcTo(0, 0, 1, 1, 180, -180);
    upRoundPath.lineTo(1, 1);
    upRoundPath.closeSubpath();

    QPainterPath downRoundPath;
    downRoundPath.moveTo(0, 0);
    downRoundPath.lineTo(0, 0.5);
    downRoundPath.arcTo(0, 0, 1, 1, 180, 180);
    downRoundPath.lineTo(1, 0);
    downRoundPath.closeSubpath();

    /* Note that construction order is important as it defines which items are on top. */
    m_manipulatorWidget = new PtzManipulatorWidget(this);

    m_zoomInButton = new PtzImageButtonWidget(this);
    m_zoomInButton->setIcon(qnSkin->icon(kZoomInIcon));
    m_zoomInButton->setToolTip(tr("Zoom In"));

    m_zoomOutButton = new PtzImageButtonWidget(this);
    m_zoomOutButton->setIcon(qnSkin->icon(kZoomOutIcon));
    m_zoomOutButton->setToolTip(tr("Zoom Out"));

    m_focusInButton = new PtzImageButtonWidget(this);
    m_focusInButton->setIcon(qnSkin->icon(kZoomInIcon));
    m_focusInButton->setToolTip(tr("Focus Far"));
    m_focusInButton->setFrameShape(Qn::CustomFrame);
    m_focusInButton->setCustomFramePath(upRoundPath);

    m_focusOutButton = new PtzImageButtonWidget(this);
    m_focusOutButton->setIcon(qnSkin->icon(kZoomOutIcon));
    m_focusOutButton->setToolTip(tr("Focus Near"));
    m_focusOutButton->setFrameShape(Qn::CustomFrame);
    m_focusOutButton->setCustomFramePath(downRoundPath);

    m_focusAutoButton = new PtzImageButtonWidget(this);
    m_focusAutoButton->setIcon(qnSkin->icon(kFocusAutoIcon));
    m_focusAutoButton->setToolTip(tr("Auto Focus"));
    m_focusAutoButton->setFrameShape(Qn::RectangularFrame);

    m_modeButton = new PtzImageButtonWidget(this);
    m_modeButton->setToolTip(tr("Change Dewarping Mode"));

    updateLayout();
    showCursor();
}

void PtzOverlayWidget::forceUpdateLayout()
{
    updateLayout();
}

void PtzOverlayWidget::hideCursor()
{
    manipulatorWidget()->setCursor(Qt::BlankCursor);
    zoomInButton()->setCursor(Qt::BlankCursor);
    zoomOutButton()->setCursor(Qt::BlankCursor);
    focusInButton()->setCursor(Qt::BlankCursor);
    focusOutButton()->setCursor(Qt::BlankCursor);
    focusAutoButton()->setCursor(Qt::BlankCursor);
}

void PtzOverlayWidget::showCursor()
{
    manipulatorWidget()->setCursor(nx::vms::client::desktop::CustomCursors::sizeAll);
    zoomInButton()->setCursor(Qt::ArrowCursor);
    zoomOutButton()->setCursor(Qt::ArrowCursor);
    focusInButton()->setCursor(Qt::ArrowCursor);
    focusOutButton()->setCursor(Qt::ArrowCursor);
    focusAutoButton()->setCursor(Qt::ArrowCursor);
}

PtzManipulatorWidget* PtzOverlayWidget::manipulatorWidget() const
{
    return m_manipulatorWidget;
}

PtzImageButtonWidget* PtzOverlayWidget::zoomInButton() const
{
    return m_zoomInButton;
}

PtzImageButtonWidget* PtzOverlayWidget::zoomOutButton() const
{
    return m_zoomOutButton;
}

PtzImageButtonWidget* PtzOverlayWidget::focusInButton() const
{
    return m_focusInButton;
}

PtzImageButtonWidget* PtzOverlayWidget::focusOutButton() const
{
    return m_focusOutButton;
}

PtzImageButtonWidget* PtzOverlayWidget::focusAutoButton() const
{
    return m_focusAutoButton;
}

PtzImageButtonWidget* PtzOverlayWidget::modeButton() const
{
    return m_modeButton;
}

Qt::Orientations PtzOverlayWidget::markersMode() const
{
    return m_markersMode;
}

void PtzOverlayWidget::setMarkersMode(Qt::Orientations mode)
{
    m_markersMode = mode;
}

void PtzOverlayWidget::setGeometry(const QRectF& rect)
{
    QSizeF oldSize = size();

    base_type::setGeometry(rect);

    if (!qFuzzyEquals(oldSize, size()))
        updateLayout();
}

void PtzOverlayWidget::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
    if (m_markersMode == 0)
    {
        base_type::paint(painter, option, widget);
        return;
    }

    QRectF rect = this->rect();

    QVector<QPointF> crosshairLines;

    QPointF center = rect.center();
    qreal unit = this->unit();

    if (m_markersMode & Qt::Horizontal)
    {
        qreal x = unit * 3.0;
        while (x < rect.width() / 2.0)
        {
            crosshairLines << center + QPointF(-x, unit / 2.0)
                           << center + QPointF(-x, -unit / 2.0);

            if (x < rect.width() / 2.0 - 3.0 * unit || !m_focusInButton->isVisible())
                crosshairLines << center + QPointF(x, unit / 2.0)
                               << center + QPointF(x, -unit / 2.0);

            x += unit;
        }
    }

    if (m_markersMode & Qt::Vertical)
    {
        qreal y = unit * 3.0;
        while (y < rect.height() / 2.0)
        {
            crosshairLines << center + QPointF(unit / 2.0, y) << center + QPointF(-unit / 2.0, y)
                           << center + QPointF(unit / 2.0, -y)
                           << center + QPointF(-unit / 2.0, -y);
            y += unit;
        }
    }

    QnScopedPainterPenRollback penRollback(painter, m_pen);
    painter->drawLines(crosshairLines);
}

qreal PtzOverlayWidget::unit() const
{
    QRectF rect = this->rect();
    return qMin(rect.width(), rect.height()) / 32;
}

void PtzOverlayWidget::updateLayout()
{
    /* We're doing manual layout of child items as this is an overlay widget and
     * we don't want layouts to clash with widget's size constraints. */

    qreal unit = this->unit();
    QRectF rect = this->rect();
    QPointF center = rect.center();
    QPointF left = (rect.topLeft() + rect.bottomLeft()) / 2.0;
    QPointF right = (rect.topRight() + rect.bottomRight()) / 2.0;
    QPointF xStep(unit, 0), yStep(0, unit);
    QSizeF size = Geometry::toSize(xStep + yStep);

    m_manipulatorWidget->setGeometry(QRectF(center - xStep - yStep, center + xStep + yStep));

    if (appContext()->localSettings()->ptzAimOverlayEnabled())
    {
        m_zoomInButton->setGeometry(QRectF(center - xStep * 3 - yStep * 2.5, 1.5 * size));
        m_zoomOutButton->setGeometry(QRectF(center + xStep * 1.5 - yStep * 2.5, 1.5 * size));
        m_modeButton->setGeometry(QRectF(left + xStep - yStep * 1.5, 3.0 * size));

        if (m_focusAutoButton->isVisible())
        {
            m_focusInButton->setGeometry(QRectF(right - xStep * 2.5 - yStep * 2.25, 1.5 * size));
            m_focusAutoButton->setGeometry(QRectF(right - xStep * 2.5 - yStep * 0.75, 1.5 * size));
            m_focusOutButton->setGeometry(QRectF(right - xStep * 2.5 + yStep * 0.75, 1.5 * size));
        }
        else
        {
            m_focusInButton->setGeometry(QRectF(right - xStep * 2.5 - yStep * 1.5, 1.5 * size));
            m_focusOutButton->setGeometry(QRectF(right - xStep * 2.5 + yStep * 0.0, 1.5 * size));
        }
    }
    else
    {
        m_zoomInButton->setGeometry(QRectF(left + xStep * 0.5 - yStep * 2.0, 1.5 * size));
        m_zoomOutButton->setGeometry(QRectF(left + xStep * 0.5 + yStep * 0.5, 1.5 * size));
        m_modeButton->setGeometry(QRectF(left + xStep * 2.5 - yStep * 1.5, 3.0 * size));

        if (m_focusAutoButton->isVisible())
        {
            m_focusInButton->setGeometry(QRectF(right - xStep * 2.0 - yStep * 2.25, 1.5 * size));
            m_focusAutoButton->setGeometry(QRectF(right - xStep * 2.0 - yStep * 0.75, 1.5 * size));
            m_focusOutButton->setGeometry(QRectF(right - xStep * 2.0 + yStep * 0.75, 1.5 * size));
        }
        else
        {
            m_focusInButton->setGeometry(QRectF(right - xStep * 2.0 - yStep * 1.5, 1.5 * size));
            m_focusOutButton->setGeometry(QRectF(right - xStep * 2.0 + yStep * 0.0, 1.5 * size));
        }
    }
}
