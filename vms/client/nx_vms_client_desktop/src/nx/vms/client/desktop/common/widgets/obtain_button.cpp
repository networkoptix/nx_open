// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "obtain_button.h"

#include <QtCore/QPropertyAnimation>
#include <QtWidgets/QStyleOptionButton>
#include <QtWidgets/QStylePainter>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>

namespace nx::vms::client::desktop {

namespace {

static const QColor kLight10Color = "#A5B7C0";
static const QColor kLight16Color = "#698796";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {{kLight10Color, "light10"}, {kLight16Color, "light16"}}},
    {QIcon::Active, {{kLight10Color, "light11"}, {kLight16Color, "light17"}}},
    {QIcon::Selected, {{kLight16Color, "light15"}}},
};

} // namespace

struct ObtainButton::Private
{
    ObtainButton* const q;
    QPropertyAnimation* rotationAnimation = nullptr;

    Private(ObtainButton* parent): q(parent)
    {
        rotationAnimation = new QPropertyAnimation(q);
        rotationAnimation->setTargetObject(q);
        rotationAnimation->setStartValue(0);
        rotationAnimation->setEndValue(360);
        rotationAnimation->setDuration(1200);
        rotationAnimation->setLoopCount(-1);
        connect(rotationAnimation, &QPropertyAnimation::valueChanged, [this]() { q->update(); });
        rotationAnimation->start();
    }
};

ObtainButton::ObtainButton(
    const QString& text,
    QWidget* parent)
    :
    base_type(
        qnSkin->icon("user_settings/spinner.svg", kIconSubstitutions),
        text,
        qnSkin->icon("user_settings/spinner.svg", kIconSubstitutions),
        text,
        parent),
    d(new Private(this))
{
}

ObtainButton::~ObtainButton()
{
}

void ObtainButton::setVisible(bool visible)
{
    if (visible != isVisible())
        visible ? d->rotationAnimation->start() : d->rotationAnimation->stop();

    base_type::setVisible(visible);
}

void ObtainButton::paintEvent(QPaintEvent* /*event*/)
{
    static QIcon emptyIcon;
    QStylePainter painter(this);
    QStyleOptionButton option;
    initStyleOption(&option);
    const auto size = option.iconSize;
    const QPixmap& pixmap = option.icon.pixmap(size);
    option.icon = emptyIcon;
    painter.drawControl(QStyle::CE_PushButton, option);
    painter.save();
    QRect pixmapRect = style()->itemPixmapRect(rect(), Qt::AlignLeft | Qt::AlignVCenter, pixmap);
    painter.translate(pixmapRect.center());
    painter.rotate(d->rotationAnimation->currentValue().toInt());
    painter.drawPixmap(-size.width()/2, -size.height()/2, pixmap);
    painter.restore();
}

} // namespace nx::vms::client::desktop
