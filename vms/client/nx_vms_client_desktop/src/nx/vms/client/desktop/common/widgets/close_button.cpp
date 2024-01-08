// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "close_button.h"

#include <QtGui/QPainter>
#include <QtWidgets/QStyle>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/icon.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/widgets/hover_button.h>
#include <nx/vms/client/desktop/style/helper.h>

namespace nx::vms::client::desktop {

static const QColor kLight10Color = "#A5B7C0";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {{kLight10Color, "light14"}}},
    {QIcon::Active, {{kLight10Color, "light14"}}},
    {QIcon::Selected, {{kLight10Color, "light14"}}},
    {QnIcon::Pressed, {{kLight10Color, "light10"}}},
};

CloseButton::CloseButton(QWidget* parent):
    HoverButton(qnSkin->icon("text_buttons/cross_close_20.svg", kIconSubstitutions), parent)
{
    setFixedSize(HoverButton::sizeHint());
}

void CloseButton::setAccented(bool accented)
{
    m_accented = accented;
}

void CloseButton::paintEvent(QPaintEvent* event)
{
    if (!m_accented)
    {
        base_type::paintEvent(event);
    }
    else
    {
        // If button is accented then it uses another color scheme, which is stored into
        // QnIcon::Pressed mode.
        QPixmap pixmap = qnSkin->icon("text_buttons/cross_close_20.svg", kIconSubstitutions)
            .pixmap(nx::style::Metrics::kDefaultIconSize, QnIcon::Pressed);
        if (pixmap.isNull())
            return;

        const auto pixmapRect = QStyle::alignedRect(
            Qt::LeftToRight, Qt::AlignCenter, pixmap.size() / pixmap.devicePixelRatio(), rect());

        QPainter painter(this);
        painter.drawPixmap(pixmapRect, pixmap);
    }
}


} // namespace nx::vms::client::desktop
