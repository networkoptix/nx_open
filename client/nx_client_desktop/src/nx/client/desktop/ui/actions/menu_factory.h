#pragma once

#include <nx/client/desktop/ui/actions/action_fwd.h>
#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_builder.h>

namespace nx {
namespace client {
namespace desktop {
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
} // namespace desktop
} // namespace client
} // namespace nx
