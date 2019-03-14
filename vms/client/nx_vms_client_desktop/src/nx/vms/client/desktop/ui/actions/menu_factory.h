#pragma once

#include <nx/vms/client/desktop/ui/actions/action_fwd.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/actions/action_builder.h>

namespace nx::vms::client::desktop {
namespace ui {
namespace action {

class MenuFactory
{
public:
    MenuFactory(Manager* menu, Action* parent);

    void beginSubMenu();
    void endSubMenu();

    void beginGroup();
    void endGroup();

    Builder registerAction(IDType id);
    Builder registerAction();
    Builder operator()(IDType id);
    Builder operator()();

private:
    Manager* m_manager;
    int m_lastFreeActionId;
    Action* m_lastAction;
    QList<Action*> m_actionStack;
    QActionGroup* m_currentGroup;
};

} // namespace action
} // namespace ui
} // namespace nx::vms::client::desktop
