#include "joystick_manager.h"
#include "abstract_joystick.h"
#include "joystick_config.h"

#include <plugins/io_device/joystick/controls/joystick_stick_control.h>

#if defined(Q_OS_WIN)
#include <plugins/io_device/joystick/drivers/mm_joystick_driver_win.h>
#include <plugins/io_device/joystick/drivers/joystick_event_filter_win.h>
#endif

#include <ui/actions/action_manager.h>
#include <ui/workbench/workbench_display.h>

namespace nx {
namespace joystick {

Manager::Manager(QnWorkbenchContext* context):
    QnWorkbenchContextAware(context)
{
}

Manager::~Manager()
{
}

void Manager::start()
{
    QnMutexLocker lock(&m_mutex);
    loadDrivers();
    loadMappings();
    applyMappingsAndCaptureJoysticks();
}

void Manager::notifyHardwareConfigurationChanged()
{
    QnMutexLocker lock(&m_mutex);
    applyMappingsAndCaptureJoysticks();
}

void Manager::loadDrivers()
{
#if defined(Q_OS_WIN)
    auto hwndId = reinterpret_cast<HWND>(mainWindow()->winId());
    m_drivers.emplace_back(new driver::MmWinDriver(hwndId));

    QApplication::instance()->installNativeEventFilter(new nx::joystick::JoystickEventFilter(hwndId));
#endif
}

void Manager::loadMappings()
{
    m_configHolder.load();
}

void Manager::applyMappings(std::vector<JoystickPtr>& joysticks)
{
    for (auto& joy: joysticks)
    { 
        auto activeConfiguration = m_configHolder.getActiveConfigurationForJoystick(
            joy->getId());

        if (!activeConfiguration)
            return;

        auto controls = joy->getControls();

        for (auto& control: controls)
        {
            auto controlId = control->getId();
            auto& eventMapping = activeConfiguration->eventMapping;

            auto rulesItr = eventMapping.find(controlId);
            if (rulesItr == eventMapping.end())
                continue;

            for (const auto& rule: rulesItr->second)
            {
                bool ruleHasBeenAdded = control->addEventHandler(
                        rule.eventType, 
                        makeEventHandler(rule, control));
                
                if (!ruleHasBeenAdded)
                    qDebug() << "Can not add rule for control" << controlId;
            }

            auto controlOverrides = m_configHolder.getControlOverrides(
                activeConfiguration->configurationId,
                controlId);

            for (const auto& controlOverride: controlOverrides)
            {
                control->applyOverride(
                    controlOverride.first,
                    controlOverride.second);
            }
        }
    }
}

void Manager::applyMappingsAndCaptureJoysticks()
{
    std::vector<nx::joystick::JoystickPtr> joysticks;
    for (const auto& drv: m_drivers)
    {
        auto joysticks = drv->enumerateJoysticks();
        applyMappings(joysticks);

        for(auto& joy: joysticks)
            drv->captureJoystick(joy);
    }
}

nx::joystick::EventHandler Manager::makeEventHandler(const mapping::Rule& rule, controls::ControlPtr control)
{
    auto handler = 
        [rule, control, this](EventType eventType, EventParameters eventParameters)
        {
            auto actionParameters = createActionParameters(
                rule,
                control,
                eventParameters);
            
            menu()->trigger(
                rule.actionType,
                actionParameters);
        };

    return handler;
}

QnActionParameters Manager::createActionParameters(
    const mapping::Rule& rule,
    const controls::ControlPtr& control,
    const EventParameters& eventParameters)
{
    QnActionParameters actionParameters;
    if (rule.eventType == EventType::stickMove)
    {
        auto stick = std::dynamic_pointer_cast<controls::Stick>(control);

        if (!stick)
            return QnActionParameters();

        auto xRange = stick->getStickOutputXRange();
        auto yRange = stick->getStickOutputYRange();
        auto zRange = stick->getStickOutputZRange();

        auto speed = QVector3D(
            (double)eventParameters.state[0] * 2 / (xRange.max - xRange.min),
            (double)eventParameters.state[1] * 2 / (yRange.max - yRange.min),
            (double)eventParameters.state[2] * 2 / (zRange.max - zRange.min));

        actionParameters.setArgument(
            Qn::ItemDataRole::PtzSpeedRole,
            speed);
    }

    for(const auto& actParameter: rule.actionParameters)
    {
        auto dataRole = fromActionParamterNameToItemDataRole(actParameter.first);
        if (!dataRole)
            continue;

        actionParameters.setArgument(
            dataRole.get(),
            fromActionParamtereValueToVariant(
                dataRole.get(),
                actParameter.second));
    }

    return actionParameters;
}

boost::optional<Qn::ItemDataRole> Manager::fromActionParamterNameToItemDataRole(
    const QString& actionParamterName) const
{
    if (actionParamterName == mapping::kPresetIndexParameterName)
        return Qn::ItemDataRole::PtzPresetIndexRole;

    return boost::none;
}

QVariant Manager::fromActionParamtereValueToVariant(
    Qn::ItemDataRole dataRole,
    const QString& actionParameterValue) const
{
    int intValue = 0;
    bool isValid = false;
    
    if (dataRole == Qn::ItemDataRole::PtzPresetIndexRole)
        intValue = actionParameterValue.toInt(&isValid);

    if (!isValid)
        return QVariant();

    return intValue;
}

} // namespace joystick
} // namespace nx