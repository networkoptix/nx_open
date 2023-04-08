// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rewind_widget.h"

#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsLinearLayout>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/scene/resource_widget/overlays/rewind_overlay.h>
#include <nx/vms/client/desktop/scene/resource_widget/overlays/rewind_triangle.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/animation/opacity_animator.h>
#include <ui/workaround/sharp_pixmap_painting.h>
#include <utils/common/event_processors.h>

constexpr int kAlpha = 76; 

namespace nx::vms::client::desktop {

class RewindWidget::BackgroundWidget: public QGraphicsWidget
{
    RewindWidget* const q;

public:
    BackgroundWidget(RewindWidget* owner):
        QGraphicsWidget(owner),
        q(owner)
    {
        setOpacity(0.0);
    }

    void paint(QPainter* painter)
    {
        const auto ratio = qApp->devicePixelRatio();
        if (q->m_size.isEmpty())
            return;

        const auto pixmapSize = QSize(q->m_size.width() * 0.4, q->m_size.height()) * ratio;
        const auto m_backgroundColor = core::colorTheme()->color("light1", kAlpha);

        painter->setPen(Qt::NoPen);
        painter->setBrush(m_backgroundColor);
        {
            if (q->m_fastForward)
            {
                QPainterPath path;
                path.addRoundedRect(0,
                    0,
                    q->m_size.width() * 0.4,
                    qCeil(q->m_size.height()),
                    q->m_size.width() * 0.2,
                    q->m_size.width() * 0.2);
                QPainterPath rectPath;
                rectPath.addRect(q->m_size.width() * 0.2,
                    0,
                    q->m_size.width() * 0.2,
                    qCeil(q->m_size.height()));
                painter->fillPath(path.united(rectPath), m_backgroundColor);
            }
            else
            {
                QPainterPath path;
                path.addRoundedRect(0,
                    0,
                    q->m_size.width() * 0.4,
                    qCeil(q->m_size.height()),
                    q->m_size.width() * 0.2,
                    q->m_size.width() * 0.2);
                QPainterPath rectPath;
                rectPath.addRect(0, 0, qCeil(q->m_size.width() * 0.2), qCeil(q->m_size.height()));
                painter->fillPath(path.united(rectPath), m_backgroundColor);
            }
        }

        q->setMinimumSize(pixmapSize / ratio);
        q->setMaximumSize(pixmapSize / ratio);
    }
};

RewindWidget::RewindWidget(QGraphicsItem* parent, bool fastForward):
    base_type(parent),
    m_fastForward(fastForward),
    m_background(new BackgroundWidget(this))
{
    setAcceptedMouseButtons(Qt::NoButton);
    {
        QSize size(nx::style::Metrics::kRewindArrowSize);
        const auto ffIcon = qnSkin->icon("item/rewind_triangle.svg");
        const auto iconPixmap = m_fastForward
            ? ffIcon.pixmap(size)
            : ffIcon.pixmap(size).transformed(QTransform().scale(-1, 1));

        m_triangle1 = new RewindTriangle(m_fastForward, this);
        m_triangle1->setVisible(true);
        m_triangle2 = new RewindTriangle(m_fastForward, this);
        m_triangle3 = new RewindTriangle(m_fastForward, this);

        auto vLayout = new QGraphicsLinearLayout(Qt::Vertical, this);
        vLayout->addStretch();
        auto layout = new QGraphicsLinearLayout(Qt::Horizontal, vLayout);
        layout->setSpacing(2);
        layout->addStretch();
        if (fastForward)
        {
            layout->addItem(m_triangle1);
            layout->addItem(m_triangle2);
            layout->addItem(m_triangle3);
        }
        else
        {
            layout->addItem(m_triangle3);
            layout->addItem(m_triangle2);
            layout->addItem(m_triangle1);
        }
        layout->addStretch();
        vLayout->addItem(layout);
        vLayout->addStretch();
    }

    connect(
        m_animationTimerListener.get(), &AnimationTimerListener::tick, this, &RewindWidget::tick);

    registerAnimation(m_animationTimerListener);
    m_animationTimerListener->startListening();
}

RewindWidget::~RewindWidget()
{
}

void RewindWidget::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/)
{
    m_background->paint(painter);
}

void RewindWidget::tick(int deltaMs)
{
    if (m_totalMs < 0)
        return;

    auto animator = opacityAnimator(m_background);
    auto animator1 = opacityAnimator(m_triangle1);
    auto animator2 = opacityAnimator(m_triangle2);
    auto animator3 = opacityAnimator(m_triangle3);

    if (!animator1 || !animator2 || !animator3 || animator1->isRunning() || !animator
        || animator2->isRunning() || animator3->isRunning() || animator->isRunning())
    {
        return;
    }

    m_totalMs += deltaMs;
    if (m_totalMs < 100)
    {
        if (m_background->opacity() == 0.0)
            m_background->setOpacity(0.1);
        animator->setTimeLimit(100);
        animator->animateTo(0.4);
        animator1->setTimeLimit(100);
        animator1->animateTo(0.5);
        return;
    }

    if (m_totalMs < 200)
    {
        animator->setTimeLimit(100);
        animator->animateTo(0.8);
        animator1->setTimeLimit(100);
        animator1->animateTo(1.0);
        animator2->setTimeLimit(100);
        animator2->animateTo(0.5);
        return;
    }

    if (m_totalMs < 300)
    {
        animator->setTimeLimit(50);
        animator->animateTo(1.0);
        animator1->setTimeLimit(100);
        animator1->animateTo(0.5);
        animator2->setTimeLimit(100);
        animator2->animateTo(1.0);
        animator3->setTimeLimit(100);
        animator3->animateTo(0.5);
        return;
    }

    if (m_totalMs < 400)
    {
        animator1->setTimeLimit(100);
        animator1->animateTo(0.0);
        animator2->setTimeLimit(100);
        animator2->animateTo(0.5);
        animator3->setTimeLimit(100);
        animator3->animateTo(1.0);
        return;
    }

    if (m_totalMs < 500)
    {
        animator2->setTimeLimit(100);
        animator2->animateTo(0.5);
        animator3->setTimeLimit(100);
        animator3->animateTo(1.0);
        return;
    }

    if (m_totalMs < 600)
    {
        animator2->setTimeLimit(100);
        animator2->animateTo(0.0);
        animator3->setTimeLimit(100);
        animator3->animateTo(0.5);
        animator->setTimeLimit(100);
        animator->animateTo(0.5);
        return;
    }

    if (m_totalMs < 700)
    {
        animator3->setTimeLimit(100);
        animator3->animateTo(0.0);
        animator->setTimeLimit(100);
        animator->animateTo(0.1);
        return;
    }

    m_background->setOpacity(0.0);
    m_totalMs = kStopped;
    setVisible(false);
}

void RewindWidget::blink()
{
    setVisible(true);
    m_totalMs = 0;
}

void RewindWidget::updateSize(QSizeF size)
{
    if (m_size != size)
    {
        m_size = size;
        update();
    }
}

} // namespace nx::vms::client::desktop
