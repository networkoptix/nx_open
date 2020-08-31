#include "keyboard_modifiers_watcher.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QWindow>

#include <utils/common/event_processors.h>

#include <nx/utils/log/log.h>

namespace nx::vms::client::desktop {

namespace {

static constexpr auto kModifierMask = Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier
    | Qt::MetaModifier | Qt::GroupSwitchModifier;

} // namespace

KeyboardModifiersWatcher::KeyboardModifiersWatcher(QObject* parent):
    base_type(parent)
{
    if (!NX_ASSERT(qApp, "Application instance must be created"))
        return;

    installEventHandler(qApp, {QEvent::KeyPress, QEvent::KeyRelease}, this,
        [this](QObject* target, QEvent* event)
        {
            if (!qobject_cast<QWindow*>(target))
                return;

            const auto keyEvent = static_cast<QKeyEvent*>(event);
            switch (keyEvent->key())
            {
                case Qt::Key_Shift:
                case Qt::Key_Control:
                case Qt::Key_Alt:
                case Qt::Key_Meta:
                case Qt::Key_AltGr:
                {
                    const auto modifiers = qApp->queryKeyboardModifiers() & kModifierMask;
                    const auto changedModifiers = m_modifiers ^ modifiers;
                    if (changedModifiers == Qt::NoModifier)
                        break;

                    m_modifiers = modifiers;
                    NX_VERBOSE(this, "Toggled keyboard modifiers %1, new modifier state: %2",
                        changedModifiers, m_modifiers);

                    emit modifiersChanged(changedModifiers);
                    break;
                }

                default:
                    break;
            }
        });

    m_modifiers = qApp->queryKeyboardModifiers() & kModifierMask;
    NX_VERBOSE(this, "Initial keyboard modifier state: %1", m_modifiers);
}

Qt::KeyboardModifiers KeyboardModifiersWatcher::modifiers() const
{
    return m_modifiers;
}

} // namespace nx::vms::client::desktop
