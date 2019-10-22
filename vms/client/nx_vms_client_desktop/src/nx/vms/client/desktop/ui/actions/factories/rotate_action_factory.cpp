#include "rotate_action_factory.h"

#include <QtCore/QCoreApplication>
#include <QtWidgets/QAction>

#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/utils/math/fuzzy.h>
#include <core/resource/layout_resource.h>

namespace {

class ContextMenu
{
    Q_DECLARE_TR_FUNCTIONS(ContextMenu)
};

qreal getRotation(QnLayoutItemIndex index)
{
    return index.layout()->getItem(index.uuid()).rotation;
}

} // namespace

namespace nx::vms::client::desktop {

RotateActionFactory::RotateActionFactory(QObject* parent):
    base_type(parent)
{
}

ui::action::Factory::ActionList RotateActionFactory::newActions(
    const ui::action::Parameters& parameters,
    QObject* parent)
{
    using namespace ui::action;

    const auto items = parameters.layoutItems();
    if (items.isEmpty())
        return ActionList();

    const qreal currentRotation =
        [items]()
        {
            const qreal result = getRotation(items.first());
            const auto predicate =
                [result](auto index) { return !qFuzzyEquals(result, getRotation(index)); };
            return std::any_of(items.begin() + 1, items.end(), predicate)
                ? -1.0
                : result;
        }();

    const auto createRotateAction =
        [this, parameters, currentRotation](const QString& text, qreal rotation)
        {
            const auto action = new QAction(text);
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

} // namespace nx::vms::client::desktop

