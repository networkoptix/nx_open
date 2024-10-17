// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "instrument_paint_syncer.h"

#include <QtCore/QEvent>
#include <QtCore/QDateTime>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

#include <ui/graphics/view/graphics_view.h>
#include <ui/graphics/view/quick_widget_container.h>

namespace {

using namespace std::chrono;

static constexpr milliseconds kOneSecond = 1000ms;

} // namespace

InstrumentPaintSyncer::InstrumentPaintSyncer(QObject* parent):
    QObject(parent),
    m_animationTimer(new QtBasedAnimationTimer(this)),
    m_currentWidget(nullptr),
    m_update(new nx::utils::PendingOperation())
{
    setFpsLimit(0);
    m_update->setFlags(nx::utils::PendingOperation::FireImmediately);
    m_update->setCallback(
        [this]()
        {
            if (m_sendPaintEvent)
            {
                QPaintEvent painEvent(QRect{});
                QApplication::sendEvent(m_currentWidget, &painEvent);
            }
            else
            {
                m_currentWidget->update();
            }
        });

    connect(m_animationTimerListener.get(), &AnimationTimerListener::tick, this,
        &InstrumentPaintSyncer::tick);

    m_animationTimer->addListener(m_animationTimerListener);
    m_animationTimerListener->startListening();
}

void InstrumentPaintSyncer::setFpsLimit(int limit)
{
    m_update->setInterval(limit > 0 ? kOneSecond / limit : 0ms);
}

bool InstrumentPaintSyncer::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() != QEvent::Paint)
        return false;

    if (currentWatched() != watched)
    {
        using namespace nx::vms::client::desktop;

        m_currentWatched = watched;
        m_currentWidget = qobject_cast<QWidget*>(watched);

        // Special case for RHI rendering - force enclosed QQuickWidget to redraw its texture,
        // but avoid repainting the whole window via update().
        // Since our instruments are monitoring paint events, do the rendering through sending
        // a paint event and let QuickWidgetContainer to issue an actual rendering call.
        m_sendPaintEvent = (bool) qobject_cast<QuickWidgetContainer*>(m_currentWidget);
    }

    updateCurrentTime(QDateTime::currentMSecsSinceEpoch());
    return false;
}

void InstrumentPaintSyncer::tick(int /*deltaTime*/)
{
    if (currentWatched() && m_currentWidget && m_currentWidget->updatesEnabled())
        m_update->requestOperation();
}

void InstrumentPaintSyncer::activatedNotify()
{
    m_animationTimer->activate();
}

void InstrumentPaintSyncer::deactivatedNotify()
{
    m_animationTimer->deactivate();
}

QObject* InstrumentPaintSyncer::currentWatched() const
{
    return m_currentWatched.data();
}
