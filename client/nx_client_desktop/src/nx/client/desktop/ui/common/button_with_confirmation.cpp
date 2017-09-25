#include "button_with_confirmation.h"

#include <QtCore/QPointer>
#include <QtCore/QPauseAnimation>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QSequentialAnimationGroup>
#include <QtGui/QInputEvent>
#include <QtWidgets/QGraphicsOpacityEffect>
#include <QtWidgets/QStyleOptionButton>
#include <QtWidgets/QStylePainter>

using namespace std::chrono;

namespace {

static const auto kDefaultConfirmationDuration = seconds(2);
static const auto kDefaultFadeOutDuration = milliseconds(500);
static const auto kDefaultFadeInDuration = milliseconds(200);

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace ui {

struct ButtonWithConfirmation::Private
{
    QString mainText;
    QString confirmationText;
    QIcon mainIcon;
    QIcon confirmationIcon;

    QSequentialAnimationGroup* animations = nullptr;
    QPauseAnimation* confirmationAnimation = nullptr;
    QPropertyAnimation* fadeOutAnimation = nullptr;
    QPropertyAnimation* fadeInAnimation = nullptr;

    QPointer<QGraphicsOpacityEffect> opacityEffect;

    enum class State
    {
        button,
        confirmation
    };

    State state = State::button;
};

ButtonWithConfirmation::ButtonWithConfirmation(QWidget* parent):
    base_type(parent),
    d(new Private())
{
    setFlat(true);

    d->opacityEffect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(d->opacityEffect);
    d->opacityEffect->setEnabled(false);

    static const auto kOpacityPropertyName = "opacity";

    d->confirmationAnimation = new QPauseAnimation(this);
    d->confirmationAnimation->setDuration(milliseconds(kDefaultConfirmationDuration).count());
    connect(d->confirmationAnimation, &QAbstractAnimation::finished,
        [this]() //< Initialize fade-out.
        {
            if (d->opacityEffect)
                d->opacityEffect->setEnabled(true);
        });

    d->fadeOutAnimation = new QPropertyAnimation(d->opacityEffect, kOpacityPropertyName, this);
    d->fadeOutAnimation->setDuration(milliseconds(kDefaultFadeOutDuration).count());
    d->fadeOutAnimation->setStartValue(1.0);
    d->fadeOutAnimation->setEndValue(0.0);
    connect(d->fadeOutAnimation, &QAbstractAnimation::finished,
        [this]() //< Switch button appearance.
        {
            d->state = Private::State::button;
            setText(d->mainText);
            setIcon(d->mainIcon);
        });

    d->fadeInAnimation = new QPropertyAnimation(d->opacityEffect, kOpacityPropertyName, this);
    d->fadeInAnimation->setDuration(milliseconds(kDefaultFadeInDuration).count());
    d->fadeInAnimation->setStartValue(0.0);
    d->fadeInAnimation->setEndValue(1.0);
    connect(d->fadeInAnimation, &QAbstractAnimation::finished,
        [this]() //< Finalize fade-in.
        {
            if (d->opacityEffect)
                d->opacityEffect->setEnabled(false);
        });

    d->animations = new QSequentialAnimationGroup(this);
    d->animations->addAnimation(d->confirmationAnimation);
    d->animations->addAnimation(d->fadeOutAnimation);
    d->animations->addAnimation(d->fadeInAnimation);

    connect(this, &QPushButton::clicked,
        [this]()
        {
            d->animations->stop();
            if (d->opacityEffect)
            {
                d->opacityEffect->setEnabled(false);
                d->opacityEffect->setOpacity(1.0);
            }

            d->state = Private::State::confirmation;
            setText(d->confirmationText);
            setIcon(d->confirmationIcon);

            d->animations->start();
        });
}

ButtonWithConfirmation::ButtonWithConfirmation(
    const QIcon& mainIcon,
    const QString& mainText,
    const QIcon& confirmationIcon,
    const QString& confirmationText,
    QWidget* parent)
    :
    ButtonWithConfirmation(parent)
{
    setMainText(mainText);
    setMainIcon(mainIcon);
    setConfirmationText(confirmationText);
    setConfirmationIcon(confirmationIcon);
}

ButtonWithConfirmation::~ButtonWithConfirmation()
{
}

QString ButtonWithConfirmation::mainText() const
{
    return d->mainText;
}

void ButtonWithConfirmation::setMainText(const QString& value)
{
    d->mainText = value;
    if (d->state == Private::State::button)
        setText(value);
}

QString ButtonWithConfirmation::confirmationText() const
{
    return d->confirmationText;
}

void ButtonWithConfirmation::setConfirmationText(const QString& value)
{
    d->confirmationText = value;
    if (d->state == Private::State::confirmation)
        setText(value);
}

QIcon ButtonWithConfirmation::mainIcon() const
{
    return d->mainIcon;
}

void ButtonWithConfirmation::setMainIcon(const QIcon& value)
{
    d->mainIcon = value;
    if (d->state == Private::State::button)
        setIcon(value);
}

QIcon ButtonWithConfirmation::confirmationIcon() const
{
    return d->confirmationIcon;
}

void ButtonWithConfirmation::setConfirmationIcon(const QIcon& value)
{
    d->confirmationIcon = value;
    if (d->state == Private::State::confirmation)
        setIcon(value);
}

milliseconds ButtonWithConfirmation::confirmationDuration() const
{
    return milliseconds(d->confirmationAnimation->duration());
}

void ButtonWithConfirmation::setConfirmationDuration(milliseconds value)
{
    return d->confirmationAnimation->setDuration(value.count());
}

milliseconds ButtonWithConfirmation::fadeOutDuration() const
{
    return milliseconds(d->fadeOutAnimation->duration());
}

void ButtonWithConfirmation::setFadeOutDuration(milliseconds value)
{
    return d->fadeOutAnimation->setDuration(value.count());
}

milliseconds ButtonWithConfirmation::fadeInDuration() const
{
    return milliseconds(d->fadeInAnimation->duration());
}

void ButtonWithConfirmation::setFadeInDuration(milliseconds value)
{
    return d->fadeInAnimation->setDuration(value.count());
}

bool ButtonWithConfirmation::event(QEvent* event)
{
    if (d->state == Private::State::button || !dynamic_cast<QInputEvent*>(event))
        return base_type::event(event);

    event->accept();
    return true;
}

void ButtonWithConfirmation::paintEvent(QPaintEvent* /*event*/)
{
    QStylePainter painter(this);
    QStyleOptionButton option;
    initStyleOption(&option);
    if (d->state == Private::State::confirmation)
        option.state |= QStyle::State_MouseOver;
    painter.drawControl(QStyle::CE_PushButton, option);
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
