// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_factory.h"

#include <memory>
#include <vector>

#include <QtCore/QTimer>
#include <QtGui/QVector3D>

#include <api/helpers/layout_id_helper.h>
#include <core/resource/layout_resource.h>

#include "descriptors.h"

namespace {

// QString.toInt() could autodetect integer base by their format (0x... for hex numbers, etc.).
constexpr int kAutoDetectBase = 0;

using namespace nx::vms::client::desktop::menu;

struct ParsedAction
{
    IDType id = NoAction;
    QString param;
};

} // namespace

namespace nx::vms::client::desktop {
namespace joystick {

QString ActionFactory::kLayoutLogicalIdParameterName = "logicalId";
QString ActionFactory::kLayoutUuidParameterName = "uuid";
QString ActionFactory::kHotkeyParameterName = "hotkey";
QString ActionFactory::kPtzPresetParameterName = "index";

struct ActionFactory::Private
{
    ActionFactory* q;

    QString modelName;

    // TODO: Make sure there is no race condition for editable lists.
    std::vector<std::vector<std::unique_ptr<ParsedAction>>> assignedActions;
    int modifierIndex = -1;

    Device::StickPosition savedStick;
    Device::ButtonStates savedButtons;
    bool actedWithModifier = false;

    // Layout focus related variables.
    double dX = 0;
    double dY = 0;
    double sumX = 0;
    double sumY = 0;
    QTimer layoutFocusTimer;

    Private(
        ActionFactory* parent,
        const JoystickDescriptor& config)
        :
        q(parent)
    {
        updateConfig(config);

        connect(&layoutFocusTimer, &QTimer::timeout, [this]{ handleLayoutFocusTimer(); });
        layoutFocusTimer.setInterval(100);
        layoutFocusTimer.start();
    }

    void updateConfig(const JoystickDescriptor& config)
    {
        modelName = config.model;

        updataButtonActionsConfig(config);

        savedStick.fill(0);

        modifierIndex = getModifierButtonIndex(config);
    }

    void updataButtonActionsConfig(const JoystickDescriptor& config)
    {
        assignedActions.clear();
        savedButtons.clear();

        for (const auto& button: config.buttons)
        {
            std::vector<std::unique_ptr<ParsedAction>> buttonActions;
            for (const auto& action: button.actions)
            {
                auto parsedAction = std::make_unique<ParsedAction>();

                parsedAction->id = action.id;

                // Parse additional parameters for known action types.
                auto it = action.parameters.end();
                switch (action.id)
                {
                    case OpenInNewTabAction:
                    {
                        it = action.parameters.find(kLayoutLogicalIdParameterName);
                        if (it == action.parameters.end())
                            it = action.parameters.find(kLayoutUuidParameterName);
                        break;
                    }
                    case PtzActivateByHotkeyAction:
                        it = action.parameters.find(kHotkeyParameterName);
                        break;
                    case PtzActivatePresetByIndexAction: //< Used for backward compatibility.
                        it = action.parameters.find(kPtzPresetParameterName);
                        break;
                    default:
                        break;
                }
                if (it != action.parameters.end())
                    parsedAction->param = it->second;

                buttonActions.push_back(std::move(parsedAction));
            }
            assignedActions.push_back(std::move(buttonActions));

            // Initialize start-up state.
            savedButtons.push_back(0);
        }
    }

    int getModifierButtonIndex(const JoystickDescriptor& config) const
    {
        for (int i = 0; i < config.buttons.size(); ++i)
        {
            for (const auto& actionDescription: config.buttons[i].actions)
            {
                if (actionDescription.id == kModifierActionId)
                    return i;
            }
        }

        return -1;
    }

    void handleStateChanged(const Device::StickPosition& stick, const Device::ButtonStates& buttons)
    {
        if (!NX_ASSERT(buttons.size() == savedButtons.size()))
            return;

        bool modifiersChanged = false;
        for (int i = 0; i < buttons.size(); ++i)
        {
            if (isModifier(i) && buttons[i] != savedButtons[i])
            {
                modifiersChanged = true;
                break;
            }
        }
        if (modifiersChanged)
            stopContinuousActions();

        // Right now we have at most one modifier button. If someday that would change, modifierState could be replaced by QBitArray.
        bool modifierIsPressed = modifierState(buttons);

        // Handle stick moves.
        if (savedStick != stick)
        {
            if (!modifierIsPressed)
            {
                setCameraSpeed(stick[Device::xIndex], stick[Device::yIndex], stick[Device::zIndex]);
            }
            else
            {
                setLayoutFocusSpeed(stick[Device::xIndex], stick[Device::yIndex], /*reset*/ false);
                setCameraSpeed(0, 0, stick[Device::zIndex]); //< Should we really zoom camera in this case?
            }
        }

        // Handle button state changes.
        for (int i = 0; i < buttons.size(); ++i)
        {
            if (buttons[i] != savedButtons[i])
            {
                if (buttons[i]) //< Pressed.
                {
                    if (isModifier(i))
                    {
                        // The modifier button just has been pressed.
                        actedWithModifier = false;
                    }
                    else
                    {
                        // Prepare and exec the action.
                        handleButton(i, modifierIsPressed);

                        // We have executed an action, so no actions assigned to modifier key should be triggered later.
                        actedWithModifier = true;
                    }
                }
                else //< Released.
                {
                    // Currently only modifier could launch actions on release.
                    if (isModifier(i) && !actedWithModifier)
                        handleButton(i, /*modifierState*/ false);

                    // Also we should stop "prolonged" actions (now they're PTZ focus in/out only).
                    auto actionId = NoAction;
                    if (const auto action = actionFor(i, modifierIsPressed))
                        actionId = action->id;
                    if (actionId == PtzFocusInAction || actionId == PtzFocusOutAction)
                        stopContinuousActions();
                }
            }
            // TODO: We could add aggregation here for the lasting button presses.
        }

        savedButtons = buttons;
        savedStick = stick;
    }

    void handleButton(int buttonId, bool modifier)
    {
        const auto action = actionFor(buttonId, modifier);
        if (!action || action->id == NoAction)
            return;

        Parameters params;
        switch (action->id)
        {
            case OpenInNewTabAction:
            {
                const auto layoutPtr = nx::layout_id_helper::findLayoutByFlexibleId(
                    q->resourcePool(),
                    action->param);
                if (layoutPtr.isNull())
                    return;

                params.setResources({layoutPtr.dynamicCast<QnResource>()});
                break;
            }
            case PtzActivateByHotkeyAction:
            {
                params.setArgument(Qn::ItemDataRole::PtzPresetIndexRole, action->param.toInt());
                params.setArgument(Qn::ItemDataRole::ForceRole, true);
                break;
            }
            case PtzActivatePresetByIndexAction:
            {
                // PtzActivatePresetByIndexAction is used by native backend, but in their case
                // it should be executed only when PTZ mode is activated for some widget.
                // Here we add ForceRole to execute the same action even if PTZ mode is not active.
                params.setArgument(Qn::ItemDataRole::PtzPresetIndexRole, action->param.toInt());
                params.setArgument(Qn::ItemDataRole::ForceRole, true);
                break;
            }
            case PtzFocusInAction:
            case PtzFocusOutAction:
            case PtzFocusAutoAction:
            {
                params.setArgument(Qn::ItemDataRole::ForceRole, true);
                break;
            }
            default:
                break; //< No additional parameters.
        }

        emit q->actionReady(action->id, params);
    }

    void stopContinuousActions()
    {
        setCameraSpeed(0, 0, 0); //< Also stops PTZ focus changes.
        setLayoutFocusSpeed(0, 0, /*reset*/ true);
    }

    void setCameraSpeed(double x, double y, double z)
    {
        // PTZ coordinate axes directed differently than on-screen coordinates used e.g. by layout.
        Parameters params;
        params.setArgument(
            Qn::ItemDataRole::PtzSpeedRole,
            QVector3D(-x, y, z));

        // Apply movement even if PTZ mode is not enabled at the active camera widget.
        params.setArgument(Qn::ItemDataRole::ForceRole, true);

        emit q->actionReady(PtzContinuousMoveAction, params);
    }

    void setLayoutFocusSpeed(double x, double y, bool reset)
    {
        if (reset)
        {
            sumX = 0;
            sumY = 0;
        }
        dX = x;
        dY = y;
    }

    void handleLayoutFocusTimer()
    {
        sumX += dX;
        sumY += dY;

        // Together with timer interval defines stick sensivity.
        const double kStep = 2.5;
        while (sumX >= kStep)
        {
            emit q->actionReady(GoToNextItemAction, {});
            actedWithModifier = true;
            sumX -= kStep;
        }
        while (sumX <= -kStep)
        {
            emit q->actionReady(GoToPreviousItemAction, {});
            actedWithModifier = true;
            sumX += kStep;
        }
        while (sumY >= kStep)
        {
            emit q->actionReady(GoToNextRowAction, {});
            actedWithModifier = true;
            sumY -= kStep;
        }
        while (sumY <= -kStep)
        {
            emit q->actionReady(GoToPreviousRowAction, {});
            actedWithModifier = true;
            sumY += kStep;
        }
    }

    bool isModifier(int id)
    {
        return modifierIndex == id;
    }

    bool modifierState(const Device::ButtonStates& buttons)
    {
        return modifierIndex >= 0
            ? buttons.at(modifierIndex)
            : false;
    }

    const ParsedAction* actionFor(int buttonId, bool modifier)
    {
        if (!NX_ASSERT(buttonId >= 0 && buttonId < assignedActions.size()))
            return nullptr;

        const auto& v = assignedActions[buttonId];

        switch (v.size())
        {
            case 0: //< No actions assigned to the button.
                return nullptr;
            case 1: //< Single action assigned.
                return modifier ? nullptr : v[0].get();
            default: //< Multiple actions.
                return modifier ? v[1].get() : v[0].get();
        }
    }
};

ActionFactory::ActionFactory(
    const JoystickDescriptor& config,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this, config))
{
}

ActionFactory::~ActionFactory()
{
    d->stopContinuousActions();
}

QString ActionFactory::modelName() const
{
    return d->modelName;
}

void ActionFactory::updateConfig(const JoystickDescriptor& config)
{
    d->updateConfig(config);
}

void ActionFactory::handleStateChanged(const Device::StickPosition& stick, const Device::ButtonStates& buttons)
{
    d->handleStateChanged(stick, buttons);
}

} // namespace joystick
} // namespace nx::vms::client::desktop
