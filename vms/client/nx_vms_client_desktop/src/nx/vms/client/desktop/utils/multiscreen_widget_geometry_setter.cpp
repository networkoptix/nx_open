// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "multiscreen_widget_geometry_setter.h"

#include <QtCore/QEvent>
#include <QtGui/QScreen>
#include <QtWidgets/QWidget>

#include <nx/utils/log/log.h>
#include <utils/screen_snaps_geometry.h>

namespace {

constexpr int kMaximumWidgetGeometrySettingAttempts = 3;

} // namespace

MultiscreenWidgetGeometrySetter::MultiscreenWidgetGeometrySetter(QWidget* target, QObject* parent):
    base_type(parent),
    m_target(target)
{
    m_target->installEventFilter(this);
}

void MultiscreenWidgetGeometrySetter::changeGeometry(const QnScreenSnaps& screenSnaps)
{
    m_screenSnaps = screenSnaps;
    const auto geometry = screenSnapsToWidgetGeometry(m_screenSnaps, m_target);
    m_finished = false;
    m_attempts = kMaximumWidgetGeometrySettingAttempts;

    m_target->setGeometry(geometry);
}

bool MultiscreenWidgetGeometrySetter::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_target && (event->type() == QEvent::Move || event->type() == QEvent::Resize))
    {
        if (!m_finished)
        {
            auto targetGeometry = screenSnapsToWidgetGeometry(m_screenSnaps, m_target);
            const auto currentGeometry = m_target->geometry();

            if (currentGeometry == targetGeometry)
            {
                m_finished = true;
            }
            else
            {
                if (--m_attempts == 0)
                {
                    NX_WARNING(m_target, "Failed to set widget geometry to ", targetGeometry);
                    m_finished = true;
                }
                else
                {
                    const auto screen = widgetScreenForGeometry(m_target, targetGeometry);
                    if (screen != m_target->screen())
                    {
                        m_target->setScreen(screen);
                        targetGeometry = screenSnapsToWidgetGeometry(m_screenSnaps, m_target);
                    }
                    m_target->setGeometry(targetGeometry);
                }
            }
        }
    }

    return base_type::eventFilter(watched, event);
}

QScreen* MultiscreenWidgetGeometrySetter::widgetScreenForGeometry(
    QWidget* widget,
    const QRect& newGeometry)
{
    // TODO: It's not working on macOS, because physical dimensions used there.

    QScreen* currentScreen = widget->screen();

    if (!NX_ASSERT(currentScreen))
        return nullptr;

    QScreen* expectedNewScreen = currentScreen;

    const QPoint center = newGeometry.center();

    if (currentScreen->geometry().contains(center))
        return expectedNewScreen;

    const auto screens = currentScreen->virtualSiblings();
    for (auto&& screen: screens)
    {
        if (screen->geometry().contains(center))
            return screen;

        if (screen->geometry().intersects(newGeometry))
            expectedNewScreen = screen;
    }

    return expectedNewScreen;
}
