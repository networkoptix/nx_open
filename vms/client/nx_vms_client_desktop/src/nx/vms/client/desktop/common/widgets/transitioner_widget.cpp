#include "transitioner_widget.h"

#include <QtCore/QSequentialAnimationGroup>
#include <QtGui/QPainter>

#include <ui/animation/accessor_animation.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr int kDefaultFadeInDurationMs = 100;
static constexpr int kDefaultFadeOutDurationMs = 100;

} // namespace

TransitionerWidget::TransitionerWidget(QWidget* parent):
    base_type(parent),
    m_fadeOut(createAnimation()),
    m_fadeIn(createAnimation()),
    m_animationGroup(new QSequentialAnimationGroup(this))
{
    setAutoFillBackground(false);
    setAttribute(Qt::WA_TranslucentBackground);

    static constexpr qreal kBegin = 0.0;
    static constexpr qreal kEnd = 1.0;
    static constexpr qreal kOpaque = 1.0;
    static constexpr qreal kTransparent = 0.0;

    m_fadeOut->setKeyValues({{kBegin, kOpaque}, {kEnd, kTransparent}});
    m_fadeIn->setKeyValues({{kBegin, kTransparent}, {kEnd, kOpaque}});

    m_fadeOut->setDuration(kDefaultFadeOutDurationMs);
    m_fadeIn->setDuration(kDefaultFadeInDurationMs);

    m_fadeOut->setEasingCurve(QEasingCurve::Linear);
    m_fadeIn->setEasingCurve(QEasingCurve::Linear);

    m_animationGroup->addAnimation(m_fadeOut);
    m_animationGroup->addAnimation(m_fadeIn);

    auto bindPixmapToAnimation =
        [this](QAbstractAnimation* animation, const QPixmap TransitionerWidget::* pixmapField)
        {
            connect(animation, &QAbstractAnimation::stateChanged, this,
                [this, animation, pixmapField](QAbstractAnimation::State newState)
                {
                    if (newState == QAbstractAnimation::Running)
                    {
                        if (animation->currentTime() == 0) //< Don't change pixmap if rewound.
                            m_current = this->*pixmapField;

                        if (m_current.isNull())
                            animation->setCurrentTime(animation->duration()); //< Skip.
                    }
                });
        };

    bindPixmapToAnimation(m_fadeOut, &TransitionerWidget::m_source);
    bindPixmapToAnimation(m_fadeIn, &TransitionerWidget::m_target);

    connect(m_animationGroup, &QAbstractAnimation::finished, this, &TransitionerWidget::finished);
}

TransitionerWidget::~TransitionerWidget()
{
}

void TransitionerWidget::setSource(const QPixmap& pixmap)
{
    m_source = pixmap;
}

void TransitionerWidget::setTarget(const QPixmap& pixmap)
{
    m_target = pixmap;

    if (m_fadeIn->state() == QAbstractAnimation::Running)
    {
        const auto remainingTimeFraction = qreal(m_fadeIn->duration() - m_fadeIn->currentTime())
            / m_fadeIn->duration();

        m_animationGroup->setCurrentTime(m_fadeOut->duration() * remainingTimeFraction);
    }
}

void TransitionerWidget::start()
{
    if (!running())
        m_animationGroup->start();
}

bool TransitionerWidget::running() const
{
    return m_animationGroup->state() == QAbstractAnimation::Running;
}

void TransitionerWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setOpacity(m_opacity);
    painter.drawPixmap(0, 0, m_current);
}

int TransitionerWidget::fadeOutDurationMs() const
{
    return m_fadeOut->duration();
}

void TransitionerWidget::setFadeOutDurationMs(int value)
{
    m_fadeOut->setDuration(value);
}

int TransitionerWidget::fadeInDurationMs() const
{
    return m_fadeIn->duration();
}

void TransitionerWidget::setFadeInDurationMs(int value) const
{
    m_fadeIn->setDuration(value);
}

void TransitionerWidget::setOpacity(qreal value)
{
    if (qFuzzyIsNull(value - m_opacity))
        return;

    m_opacity = value;
    update();
}

QVariantAnimation* TransitionerWidget::createAnimation()
{
    auto result = new QnAccessorAnimation(this);
    result->setAccessor(newAccessor(
        [this](const QObject*) { return m_opacity; },
        [this](QObject*, const QVariant& value) { return setOpacity(value.toReal()); }));

    result->setTargetObject(this);
    return result;
}

} // namespace nx::vms::client::desktop
