#include "action_push_button.h"

namespace nx {
namespace client {
namespace desktop {

ActionPushButton::ActionPushButton(QWidget* parent): base_type(parent)
{
    setHidden(true);
}

ActionPushButton::ActionPushButton(const CommandActionPtr& action, QWidget* parent):
    base_type(parent)
{
    setAction(action);
}

ActionPushButton::~ActionPushButton()
{
}

CommandActionPtr ActionPushButton::action() const
{
    return m_action;
}

void ActionPushButton::setAction(const CommandActionPtr& value)
{
    if (m_action == value)
        return;

    m_actionConnections.reset();

    m_action = value;
    updateFromAction();

    if (!m_action)
        return;

    if (isCheckable())
        setChecked(m_action->isChecked());

    m_actionConnections.reset(new QnDisconnectHelper());

    *m_actionConnections << connect(m_action.data(), &QAction::changed,
        this, &ActionPushButton::updateFromAction);

    *m_actionConnections << connect(m_action.data(), &QAction::toggled,
        this, &ActionPushButton::setChecked);

    *m_actionConnections << connect(this, &QPushButton::toggled,
        m_action.data(), &QAction::setChecked);

    *m_actionConnections << connect(this, &QPushButton::clicked, m_action.data(),
        [this]()
        {
            if (!isCheckable())
                m_action->trigger();
        });
}

void ActionPushButton::updateFromAction()
{
    setHidden(m_action.isNull());
    if (!m_action)
        return;

    setText(m_action->text());
    setIcon(m_action->icon());
    setToolTip(m_action->toolTip());
    setStatusTip(m_action->statusTip());
    setWhatsThis(m_action->whatsThis());
    setCheckable(m_action->isCheckable());
    setEnabled(m_action->isEnabled());
    setShortcut(m_action->shortcut());
    setShortcutAutoRepeat(m_action->autoRepeat());
    setVisible(m_action->isVisible());
}

} // namespace desktop
} // namespace client
} // namespace nx
