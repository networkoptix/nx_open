#pragma once

#include <ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_builder.h>

class QActionGroup;
class QnAction;
class QnActionManager;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace action {

class MenuFactory
{
public:
    MenuFactory(QnActionManager* menu, QnAction* parent);

    void beginSubMenu();
    void endSubMenu();

    void beginGroup();
    void endGroup();

    Builder operator()(QnActions::IDType id);
    Builder operator()();

private:
    QnActionManager *m_manager;
    int m_lastFreeActionId;
    QnAction *m_lastAction;
    QList<QnAction *> m_actionStack;
    QActionGroup* m_currentGroup;
};

} // namespace action
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
