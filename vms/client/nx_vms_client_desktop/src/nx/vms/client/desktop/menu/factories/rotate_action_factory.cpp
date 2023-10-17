// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rotate_action_factory.h"

#include <QtCore/QCoreApplication>
#include <QtGui/QAction>

#include <client/client_globals.h>
#include <core/resource/layout_resource.h>
#include <nx/utils/algorithm/same.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>

#include "../action_manager.h"
#include "../action_parameters.h"

namespace {

class ContextMenu
{
    Q_DECLARE_TR_FUNCTIONS(ContextMenu)
};

} // namespace

namespace nx::vms::client::desktop {
namespace menu {

RotateActionFactory::RotateActionFactory(Manager* parent):
    base_type(parent)
{
}

Factory::ActionList RotateActionFactory::newActions(
    const Parameters& parameters,
    QObject* parent)
{
    const auto items = parameters.layoutItems();
    if (items.isEmpty())
        return ActionList();

    const auto getter = [](auto index) { return index.layout()->getItem(index.uuid()).rotation; };
    const auto currentRotation = utils::algorithm::sameValue(items, getter, -1.0);

    const auto createRotateAction =
        [this, parameters, currentRotation, parent](const QString& text, qreal rotation)
        {
            const auto action = new QAction(text, parent);
            action->setCheckable(true);
            action->setChecked(qFuzzyEquals(currentRotation, rotation));

            connect(action, &QAction::triggered, action,
                [this, parameters, rotation]()
                {
                    Parameters actionParams(parameters);
                    actionParams.setArgument(Qn::ItemRotationRole, rotation);
                    menu()->trigger(RotateToAction, actionParams);
                });
            return action;
        };

    ActionList result;
    result.append(createRotateAction(ContextMenu::tr("0 degrees"), 0));
    result.append(createRotateAction(ContextMenu::tr("90 degrees"), 90));
    result.append(createRotateAction(ContextMenu::tr("180 degrees"), 180));
    result.append(createRotateAction(ContextMenu::tr("270 degrees"), 270));
    return result;
}

} // namespace menu
} // namespace nx::vms::client::desktop
