#include "menu_factory.h"

#include <QtWidgets/QActionGroup>

#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>

namespace nx::vms::client::desktop {
namespace ui {
namespace action {

MenuFactory::MenuFactory(Manager* menu, Action* parent):
    m_manager(menu),
    m_lastFreeActionId(ActionCount),
    m_currentGroup(0)
{
    m_actionStack.push_back(parent);
    m_lastAction = parent;
}

void MenuFactory::beginSubMenu()
{
    m_actionStack.push_back(m_lastAction);
}

void MenuFactory::endSubMenu()
{
    m_actionStack.pop_back();
}

void MenuFactory::beginGroup()
{
    m_currentGroup = new QActionGroup(m_manager);
}

void MenuFactory::endGroup()
{
    m_currentGroup = nullptr;
}

Builder MenuFactory::registerAction(IDType id)
{
    auto action = m_manager->action(id);
    if (!action)
    {
        action = new Action(id, m_manager);
        m_manager->registerAction(action);
    }

    auto parentAction = m_actionStack.back();
    parentAction->addChild(action);
    parentAction->setFlags(parentAction->flags() | RequiresChildren);

    m_lastAction = action;
    if (m_currentGroup)
        m_currentGroup->addAction(action);

    return Builder(action);
}

Builder MenuFactory::registerAction()
{
    return registerAction(static_cast<IDType>(m_lastFreeActionId++));
}

Builder MenuFactory::operator()(IDType id)
{
    return registerAction(id);
}

Builder MenuFactory::operator()()
{
    return registerAction();
}

} // namespace action
} // namespace ui
} // namespace nx::vms::client::desktop
