// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "old_style.h"

#include <cmath> /* For std::fmod. */

#include <QtCore/QSet>
#include <QtGui/QAction>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsWidget>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/QToolBar>

#include <nx/build_info.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/skin/font_config.h>
#include <ui/common/text_pixmap_cache.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/util.h>
#include <utils/common/variant.h>
#include <utils/math/color_transformations.h>
#include <utils/math/linear_combination.h>

namespace nx::vms::client::desktop {

namespace {

const char* qn_hoveredPropertyName = "_qn_hovered";

// We don't have icons for 3x scaling. Thus we have to turn on smooth mode
// on painter.
nx::utils::SharedGuardPtr make3xHiDpiWorkaround(QPainter* painter)
{
    if (!painter || !painter->device() || painter->device()->devicePixelRatio() <= 2)
        return nullptr;

    const bool isSmooth = painter->testRenderHint(QPainter::SmoothPixmapTransform);
    if (isSmooth)
        return nullptr;

    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    return nx::utils::makeSharedGuard(
        [painter]() { painter->setRenderHint(QPainter::SmoothPixmapTransform, false); });
}

} // namespace

// ------------------------------------------------------------------------------------------------
// OldStyle

OldStyle::OldStyle(QStyle* style):
    base_type(style),
    m_skin(qnSkin)
{
}

QPixmap OldStyle::generatedIconPixmap(
    QIcon::Mode iconMode,
    const QPixmap& pixmap,
    const QStyleOption* option) const
{
    if (iconMode == QIcon::Disabled)
    {
        QImage image = QImage(pixmap.size(), QImage::Format_ARGB32);
        image.fill(qRgba(0, 0, 0, 0));
        image.setDevicePixelRatio(pixmap.devicePixelRatio());

        QPainter painter(&image);
        painter.setOpacity(nx::build_info::isLinux() ? 0.6 : 0.3);
        painter.drawPixmap(0, 0, pixmap);
        painter.end();

        return QPixmap::fromImage(image);
    }

    return base_type::generatedIconPixmap(iconMode, pixmap, option);
}

void OldStyle::drawComplexControl(ComplexControl control,
    const QStyleOptionComplex* option,
    QPainter* painter,
    const QWidget* widget) const
{
    const auto workaround = make3xHiDpiWorkaround(painter);

    switch (control)
    {
        case CC_TitleBar:
        {
            /* Draw background in order to avoid painting artifacts. */
            QColor bgColor =
                option->palette.color(widget ? widget->backgroundRole() : QPalette::Window);
            if (bgColor != Qt::transparent)
            {
                bgColor.setAlpha(255);
                painter->fillRect(option->rect, bgColor);
            }
            break;
        }
        default:
            break;
    }

    base_type::drawComplexControl(control, option, painter, widget);
}

void OldStyle::drawControl(ControlElement element,
    const QStyleOption* option,
    QPainter* painter,
    const QWidget* widget) const
{
    const auto workaround = make3xHiDpiWorkaround(painter);

    switch (element)
    {
        case CE_MenuItem:
            if (drawMenuItemControl(option, painter, widget))
                return;
            break;
        case CE_ItemViewItem:
            if (drawItemViewItemControl(option, painter, widget))
                return;
            break;
        default:
            break;
    }

    base_type::drawControl(element, option, painter, widget);
}

void OldStyle::drawPrimitive(PrimitiveElement element,
    const QStyleOption* option,
    QPainter* painter,
    const QWidget* widget) const
{
    const auto workaround = make3xHiDpiWorkaround(painter);
    base_type::drawPrimitive(element, option, painter, widget);
}

int OldStyle::styleHint(StyleHint hint,
    const QStyleOption* option,
    const QWidget* widget,
    QStyleHintReturn* returnData) const
{
    switch (hint)
    {
        case SH_ToolTipLabel_Opacity:
            return 255;
        default:
            break;
    }

    return base_type::styleHint(hint, option, widget, returnData);
}

void OldStyle::polish(QApplication* application)
{
    base_type::polish(application);

    QFont font = fontConfig()->normal();
    font.setStyle(QFont::StyleNormal);
    font.setFamily("Roboto");
    application->setFont(font);

    application->setEffectEnabled(Qt::UI_AnimateCombo, false);
}

void OldStyle::polish(QWidget* widget)
{
    base_type::polish(widget);

    // QWidget::scroll method has caching issues leading to some garbage drawn in the updated
    // areas. As a workaround we force it to repaint all contents.
    if (const auto headerView = qobject_cast<QHeaderView*>(widget))
        headerView->viewport()->setAutoFillBackground(false);

    if (const auto button = qobject_cast<QAbstractButton*>(widget))
        button->setIcon(m_skin->icon(button->icon()));
}

// -------------------------------------------------------------------------- //
// Painting
// -------------------------------------------------------------------------- //
bool OldStyle::drawMenuItemControl(
    const QStyleOption* option,
    QPainter* painter,
    const QWidget* widget) const
{
    const auto workaround = make3xHiDpiWorkaround(painter);

    const auto itemOption = qstyleoption_cast<const QStyleOptionMenuItem*>(option);
    if (!itemOption)
        return false;

    const auto menu = qobject_cast<const QMenu*>(widget);
    if (!menu)
        return false;

    // There are cases when we want an action to be checkable, but do not want the checkbox
    // displayed in the menu. So we introduce an internal property for this.
    QAction* action = menu->actionAt(option->rect.center());
    if (!action || !action->property(kHideCheckBoxInMenu).toBool())
        return false;

    auto localOption = *itemOption;
    localOption.checkType = QStyleOptionMenuItem::NotCheckable;
    base_type::drawControl(CE_MenuItem, &localOption, painter, widget);
    return true;
}

bool OldStyle::drawItemViewItemControl(
    const QStyleOption* option,
    QPainter* painter,
    const QWidget* widget) const
{
    const auto workaround = make3xHiDpiWorkaround(painter);

    const auto itemOption = qstyleoption_cast<const QStyleOptionViewItem*>(option);
    if (!itemOption)
        return false;

    const auto view = qobject_cast<const QAbstractItemView*>(widget);
    if (!view)
        return false;

    const QWidget* editor = view->indexWidget(itemOption->index);
    if (!editor)
        return false;

    // If an editor is opened, don'h draw item's text. Editor background may be transparent, and
    // item text will shine through.

    auto localOption = *itemOption;
    localOption.text = QString();
    base_type::drawControl(CE_ItemViewItem, &localOption, painter, widget);
    return true;
}

} // namespace nx::vms::client::desktop
