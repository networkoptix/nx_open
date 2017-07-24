#include "button_with_confirmation.h"

#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtCore/QPauseAnimation>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QSequentialAnimationGroup>
#include <QtWidgets/QGraphicsOpacityEffect>

namespace {

static const std::chrono::milliseconds kDefaultConfirmationDurationMs(2000);
static const std::chrono::milliseconds kDefaultFadeOutDurationMs(500);
static const std::chrono::milliseconds kDefaultFadeInDurationMs(200);

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace ui {

struct ButtonWithConfirmationPrivate
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
    m_d(new ButtonWithConfirmationPrivate())
{
    m_d->opacityEffect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(m_d->opacityEffect);

    static const auto kOpacityPropertyName = "opacity";

    m_d->confirmationAnimation = new QPauseAnimation(this);
    m_d->confirmationAnimation->setDuration(kDefaultConfirmationDurationMs.count());
    connect(m_d->confirmationAnimation, &QAbstractAnimation::finished,
        [this]() //< Initialize fade-out.
        {
            if (m_d->opacityEffect)
                m_d->opacityEffect->setEnabled(true);
        });

    m_d->fadeOutAnimation = new QPropertyAnimation(m_d->opacityEffect, kOpacityPropertyName, this);
    m_d->fadeOutAnimation->setDuration(kDefaultFadeOutDurationMs.count());
    m_d->fadeOutAnimation->setStartValue(1.0);
    m_d->fadeOutAnimation->setEndValue(0.0);
    connect(m_d->fadeOutAnimation, &QAbstractAnimation::finished,
        [this]() //< Switch button appearance.
        {
            m_d->state = ButtonWithConfirmationPrivate::State::button;
            setText(m_d->mainText);
            setIcon(m_d->mainIcon);
        });

    m_d->fadeInAnimation = new QPropertyAnimation(m_d->opacityEffect, kOpacityPropertyName, this);
    m_d->fadeInAnimation->setDuration(kDefaultFadeInDurationMs.count());
    m_d->fadeInAnimation->setStartValue(0.0);
    m_d->fadeInAnimation->setEndValue(1.0);
    connect(m_d->fadeInAnimation, &QAbstractAnimation::finished,
        [this]() //< Finalize fade-in.
        {
            if (m_d->opacityEffect)
                m_d->opacityEffect->setEnabled(false);
        });

    m_d->animations = new QSequentialAnimationGroup(this);
    m_d->animations->addAnimation(m_d->confirmationAnimation);
    m_d->animations->addAnimation(m_d->fadeOutAnimation);
    m_d->animations->addAnimation(m_d->fadeInAnimation);

    connect(this, &QPushButton::clicked,
        [this]()
        {
            m_d->animations->stop();
            if (m_d->opacityEffect)
            {
                m_d->opacityEffect->setEnabled(false);
                m_d->opacityEffect->setOpacity(1.0);
            }

            m_d->state = ButtonWithConfirmationPrivate::State::confirmation;
            setText(m_d->confirmationText);
            setIcon(m_d->confirmationIcon);

            m_d->animations->start();
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
    return m_d->mainText;
}

void ButtonWithConfirmation::setMainText(const QString& value)
{
    m_d->mainText = value;
    if (m_d->state == ButtonWithConfirmationPrivate::State::button)
        setText(value);
}

QString ButtonWithConfirmation::confirmationText() const
{
    return m_d->confirmationText;
}

void ButtonWithConfirmation::setConfirmationText(const QString& value)
{
    m_d->confirmationText = value;
    if (m_d->state == ButtonWithConfirmationPrivate::State::confirmation)
        setText(value);
}

QIcon ButtonWithConfirmation::mainIcon() const
{
    return m_d->mainIcon;
}

void ButtonWithConfirmation::setMainIcon(const QIcon& value)
{
    m_d->mainIcon = value;
    if (m_d->state == ButtonWithConfirmationPrivate::State::button)
        setIcon(value);
}

QIcon ButtonWithConfirmation::confirmationIcon() const
{
    return m_d->confirmationIcon;
}

void ButtonWithConfirmation::setConfirmationIcon(const QIcon& value)
{
    m_d->confirmationIcon = value;
    if (m_d->state == ButtonWithConfirmationPrivate::State::confirmation)
        setIcon(value);
}

std::chrono::milliseconds ButtonWithConfirmation::confirmationDuration() const
{
    return std::chrono::milliseconds(m_d->confirmationAnimation->duration());
}

void ButtonWithConfirmation::setConfirmationDuration(std::chrono::milliseconds value)
{
    return m_d->confirmationAnimation->setDuration(value.count());
}

std::chrono::milliseconds ButtonWithConfirmation::fadeOutDuration() const
{
    return std::chrono::milliseconds(m_d->fadeOutAnimation->duration());
}

void ButtonWithConfirmation::setFadeOutDuration(std::chrono::milliseconds value)
{
    return m_d->fadeOutAnimation->setDuration(value.count());
}

std::chrono::milliseconds ButtonWithConfirmation::fadeInDuration() const
{
    return std::chrono::milliseconds(m_d->fadeInAnimation->duration());
}

void ButtonWithConfirmation::setFadeInDuration(std::chrono::milliseconds value)
{
    return m_d->fadeInAnimation->setDuration(value.count());
}

bool ButtonWithConfirmation::event(QEvent* event)
{
    if (m_d->state == ButtonWithConfirmationPrivate::State::button)
        return base_type::event(event);

    switch (event->type())
    {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseMove:
        case QEvent::Wheel:
        case QEvent::TabletMove:
        case QEvent::TabletPress:
        case QEvent::TabletRelease:
        case QEvent::TouchBegin:
        case QEvent::TouchEnd:
        case QEvent::TouchUpdate:
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
        case QEvent::HoverEnter:
        case QEvent::HoverLeave:
        case QEvent::HoverMove:
        case QEvent::Enter:
        case QEvent::Leave:
            event->accept();
            return true;

        default:
            return base_type::event(event);
    }
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
