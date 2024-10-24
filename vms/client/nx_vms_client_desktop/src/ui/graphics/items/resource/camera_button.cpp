// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtGui/QPainter>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/icon.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/style/software_trigger_pixmaps.h>

#include "camera_button.h"

namespace nx::vms::client::desktop {

static constexpr qreal kFrameLineWidth = 2.0;
static const QSize kDefaultButtonSize(40, 40);

//-------------------------------------------------------------------------------------------------

struct CameraButton::Private
{
    CameraButton* const q;

    QColor normalColor;
    QColor activeColor;
    QColor pressedColor;
    QString iconName;
    QSize buttonSize;
    State state = State::Default;
    bool isLive = true;
    bool prolonged = false;

    void updateIcon();
};

void CameraButton::Private::updateIcon()
{
    const auto buttonPixmap = SoftwareTriggerPixmaps::pixmapByName(iconName);

    const auto generateStatePixmap =
        [this, buttonPixmap] (const QColor& backgroundColor)
        {
            return q->generatePixmap(backgroundColor, QColor(), buttonPixmap);
        };

    QIcon icon;
    const auto normalPixmap = generateStatePixmap(q->normalColor());
    icon.addPixmap(normalPixmap, QIcon::Normal);
    icon.addPixmap(normalPixmap, QIcon::Disabled);
    icon.addPixmap(generateStatePixmap(q->activeColor()), QIcon::Active);
    icon.addPixmap(generateStatePixmap(q->pressedColor()), QnIcon::Pressed);

    q->setIcon(icon);
}

//-------------------------------------------------------------------------------------------------

CameraButton::CameraButton(QGraphicsItem* parent,
    const std::optional<QColor>& pressedColor,
    const std::optional<QColor>& activeColor,
    const std::optional<QColor>& normalColor)
    :
    base_type(parent),
    d(new Private{.q = this})
{
    d->pressedColor = pressedColor.value_or(core::colorTheme()->color("brand", 128));
    d->activeColor = activeColor.value_or(core::colorTheme()->color("brand", 179));
    d->normalColor = normalColor.value_or(core::colorTheme()->color("@dark1", 128));

    setButtonSize(kDefaultButtonSize);
}

CameraButton::~CameraButton()
{

}

QString CameraButton::iconName() const
{
    return d->iconName;
}

void CameraButton::setIconName(const QString& name)
{
    const auto iconName = SoftwareTriggerPixmaps::effectivePixmapName(name);
    if (d->iconName == iconName)
        return;

    d->iconName = iconName;
    d->updateIcon();
}

CameraButton::State CameraButton::state() const
{
    return d->state;
}

void CameraButton::setState(State state)
{
    if (state == d->state)
        return;

    d->state = state;
    emit stateChanged();
}

bool CameraButton::isLive() const
{
    return d->isLive;
}

void CameraButton::setLive(bool value)
{
    if (value == d->isLive)
        return;

    d->isLive = value;
    emit isLiveChanged();
}

bool CameraButton::prolonged() const
{
    return d->prolonged;
}

void CameraButton::setProlonged(bool value)
{
    if (d->prolonged == value)
        return;

    d->prolonged = value;

    emit prolongedChanged();
}

QSize CameraButton::buttonSize() const
{
    return d->buttonSize;
}

void CameraButton::setButtonSize(const QSize& value)
{
    if (d->buttonSize == value)
        return;

    d->buttonSize = value;
    setFixedSize(d->buttonSize);
    d->updateIcon();
}

QColor CameraButton::normalColor()
{
    return d->normalColor;
}

QColor CameraButton::activeColor()
{
    return d->activeColor;
}

QColor CameraButton::pressedColor() const
{
    return d->pressedColor;
}

QPixmap CameraButton::generatePixmap(
    const QColor& background,
    const QColor& frame,
    const QPixmap& icon)
{
    const auto pixelRatio = qApp->devicePixelRatio();

    QPixmap target(d->buttonSize * pixelRatio);
    target.setDevicePixelRatio(pixelRatio);
    target.fill(Qt::transparent);

    QPainter painter(&target);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(background.isValid() ? QBrush(background) : QBrush(Qt::NoBrush));
    painter.setPen(frame.isValid() ? QPen(frame, kFrameLineWidth) : QPen(Qt::NoPen));

    const QRect buttonRect(QPoint{}, d->buttonSize);
    painter.drawEllipse(frame.isValid()
            ? nx::vms::client::core::Geometry::eroded(QRectF(buttonRect), kFrameLineWidth / 2.0)
            : buttonRect);

    if (!icon.isNull())
    {
        const auto pixmapRect = nx::vms::client::core::Geometry::aligned(
            icon.size() / icon.devicePixelRatio(),
            buttonRect);

        painter.drawPixmap(pixmapRect, icon);
    }

    return target;
}

} // namespace nx::vms::client::desktop
