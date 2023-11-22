// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_indicator_item.h"

#include <chrono>

#include <QtCore/QTimer>
#include <QtGui/QPainter>
#include <QtWidgets/QGraphicsLinearLayout>

#include <qt_graphics_items/graphics_label.h>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/common/widgets/busy_indicator.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/skin/font_config.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/animation/opacity_animator.h>
#include <utils/common/scoped_painter_rollback.h>

using namespace std::chrono;
using namespace nx::vms::client;
using namespace nx::vms::client::desktop;

namespace {

static constexpr auto kFadeInDuration = 200ms;
static constexpr auto kFadeOutDuration = 1s;
static const QMargins kMargins(11, 3, 8, 3);

void updateOpacity(QnActionIndicatorItem* target, bool opaque)
{
    const auto duration = opaque ? milliseconds(kFadeInDuration) : milliseconds(kFadeOutDuration);
    const auto animator = opacityAnimator(target);
    animator->setDurationOverride(duration.count());
    animator->animateTo(opaque ? 1.0 : 0.0);
}

} // namespace

struct QnActionIndicatorItem::Private
{
    BusyIndicatorGraphicsWidget* const indicator;
    GraphicsLabel* const label;
    QTimer* const timer;
    QString text;
};

QnActionIndicatorItem::QnActionIndicatorItem(QGraphicsWidget* parent):
    base_type(parent),
    d(new Private{new BusyIndicatorGraphicsWidget(this), new GraphicsLabel(this), new QTimer(this)})
{
    const auto layout = new QGraphicsLinearLayout(Qt::Horizontal, this);
    layout->addItem(d->indicator);
    layout->addItem(d->label);
    layout->setContentsMargins(kMargins.left(), kMargins.top(), kMargins.right(), kMargins.bottom());
    layout->setSpacing(11);

    d->indicator->dots()->setDotRadius(2);
    d->indicator->dots()->setDotSpacing(3);
    d->indicator->setIndicatorColor(core::colorTheme()->color("light1"));

    QFont font;
    fontConfig()->small().pixelSize();
    font.setWeight(QFont::Medium);
    d->label->setProperty(nx::style::Properties::kDontPolishFontProperty, true);
    d->label->setFont(font);

    d->timer->setSingleShot(true);
    connect(d->timer, &QTimer::timeout, this, [this]() { setText({}); });

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setOpacity(0.0);
}

QnActionIndicatorItem::~QnActionIndicatorItem()
{
    // Required here for forward-declared scoped pointer destruction.
}

QString QnActionIndicatorItem::text() const
{
    return d->text;
}

void QnActionIndicatorItem::setText(const QString& value)
{
    d->timer->stop();

    if (d->text == value)
        return;

    d->text = value;

    if (!value.isEmpty())
        d->label->setText(value);

    updateOpacity(this, /*opaque*/ !value.isEmpty());
}

void QnActionIndicatorItem::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/)
{
    static constexpr qreal kRoundingRadius = 2.0;
    static const QBrush kBackgroundBrush(core::colorTheme()->color("dark1", 127));

    QnScopedPainterPenRollback penRollback(painter, Qt::NoPen);
    QnScopedPainterBrushRollback brushRollback(painter, kBackgroundBrush);
    QnScopedPainterOpacityRollback opacityRollback(painter, opacity());
    painter->drawRoundedRect(rect(), kRoundingRadius, kRoundingRadius);
}

void QnActionIndicatorItem::clear(std::chrono::milliseconds after)
{
    if (after <= 0ms)
        setText({});
    else
        d->timer->start(after);
}
