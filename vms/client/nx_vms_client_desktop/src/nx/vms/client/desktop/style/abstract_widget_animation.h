// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtWidgets/QWidget>

#include <ui/animation/animation_timer.h>

namespace nx::vms::client::desktop {

class AbstractWidgetAnimation: public QObject
{
    Q_OBJECT

public:
    AbstractWidgetAnimation(QObject* parent = nullptr):
        QObject(parent)
    {
        auto animationTimer = new QtBasedAnimationTimer(this);
        connect(animationTimer, &QtBasedAnimationTimer::tick, this,
            &AbstractWidgetAnimation::tick);
        animationTimer->start();
    }

    void start(const QWidget* widget, qreal speed, qreal value)
    {
        if (!widget)
            return;

        if (!m_connectedWidgets.contains(widget))
        {
            m_connectedWidgets.insert(widget);
            connect(widget, &QObject::destroyed, this,
                &AbstractWidgetAnimation::at_widget_destroyed);
        }

        m_animationByWidget[widget] = {value, speed, true};
    }

    void stop(const QWidget* widget)
    {
        auto pos = m_animationByWidget.find(widget);
        if (pos == m_animationByWidget.end())
            return;

        pos->running = false;
    }

    qreal value(const QWidget* widget, qreal defaultValue = 0.0) const
    {
        auto pos = m_animationByWidget.find(widget);
        if (pos == m_animationByWidget.end())
            return defaultValue;

        return pos->value;
    }

    void setValue(const QWidget* widget, qreal value)
    {
        m_animationByWidget[widget].value = value;
    }

    bool isRunning(const QWidget* widget)
    {
        auto pos = m_animationByWidget.find(widget);
        if (pos == m_animationByWidget.end())
            return false;

        return pos->running;
    }

private:
    void tick(int deltaMs)
    {
        for (auto pos = m_animationByWidget.begin(); pos != m_animationByWidget.end(); ++pos)
        {
            if (!pos->running)
                continue;

            const_cast<QWidget*>(pos.key())->update();
            pos->value += pos->speed * deltaMs / 1000.0;
        }
    }

    void at_widget_destroyed(QObject* widget)
    {
        m_animationByWidget.remove(static_cast<const QWidget*>(widget));
        m_connectedWidgets.remove(static_cast<const QWidget*>(widget));
    }

private:
    struct Animation
    {
        qreal value = 0.0;
        qreal speed = 0.0;
        bool running = false;
    };

    QSet<const QWidget*> m_connectedWidgets;
    QHash<const QWidget*, Animation> m_animationByWidget;
};

} // namespace nx::vms::client::desktop
