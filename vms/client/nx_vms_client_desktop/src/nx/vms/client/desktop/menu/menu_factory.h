// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "action_builder.h"
#include "action_fwd.h"
#include "actions.h"

namespace nx::vms::client::desktop {
namespace menu {

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

} // namespace menu
} // namespace nx::vms::client::desktop
