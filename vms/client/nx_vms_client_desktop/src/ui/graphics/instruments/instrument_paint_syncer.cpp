// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "instrument_paint_syncer.h"

#include <QtCore/QEvent>
#include <QtCore/QDateTime>
#include <QtWidgets/QWidget>

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
    m_update->setCallback([this]() { m_currentWidget->update(); });

    m_animationTimer->addListener(this);
    startListening();
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
        m_currentWatched = watched;
        m_currentWidget = qobject_cast<QWidget*>(watched);
    }

    updateCurrentTime(QDateTime::currentMSecsSinceEpoch());
    return false;
}

void InstrumentPaintSyncer::tick(int /*deltaTime*/)
{
    if (currentWatched() && m_currentWidget)
    {
       m_update->requestOperation();
    }
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
