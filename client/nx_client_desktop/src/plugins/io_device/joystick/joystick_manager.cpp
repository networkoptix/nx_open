#include "joystick_manager.h"
#include "abstract_joystick.h"
#include "joystick_config.h"

#include <QtCore/QStandardPaths>

#include <QtGui/QVector3D>

#include <QtWidgets/QApplication>

#include <plugins/io_device/joystick/controls/joystick_stick_control.h>

#if defined(Q_OS_WIN)
#include <plugins/io_device/joystick/drivers/mm_joystick_driver_win.h>
#include <plugins/io_device/joystick/drivers/joystick_event_filter_win.h>
#endif

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/workbench/workbench_display.h>

namespace {

const QString kDefaultConfigFileName = lit("joystick_config.json");

} // namespace

namespace nx {
namespace client {
namespace plugins {
namespace io_device {
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

    bool loaded = loadMappings();
    NX_ASSERT(loaded);
    if (!loaded)
        return;

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

    QApplication::instance()->installNativeEventFilter(new driver::JoystickEventFilter(hwndId));
#endif
}

bool Manager::loadMappings()
{
    const auto path = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + L'/' + kDefaultConfigFileName;
    bool loaded = m_configHolder.load(path);

    if (!loaded)
        loaded = m_configHolder.load(lit(":/%1").arg(kDefaultConfigFileName));

    return loaded;
}

void Manager::applyMappings(std::vector<JoystickPtr>& joysticks)
{
    for (auto& joy: joysticks)
    {
        auto activeConfiguration = m_configHolder.getActiveConfigurationForJoystick(
            joy->id());

        if (!activeConfiguration)
            return;

        auto controls = joy->controls();

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

            auto controlOverrides = m_configHolder.controlOverrides(
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
    std::vector<JoystickPtr> joysticks;
    for (const auto& drv: m_drivers)
    {
        auto joysticks = drv->enumerateJoysticks();
        applyMappings(joysticks);

        for (auto& joy: joysticks)
            drv->captureJoystick(joy);
    }
}

EventHandler Manager::makeEventHandler(const config::Rule& rule, controls::ControlPtr control)
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

nx::client::desktop::ui::action::Parameters Manager::createActionParameters(
    const config::Rule& rule,
    const controls::ControlPtr& control,
    const EventParameters& eventParameters)
{
    nx::client::desktop::ui::action::Parameters actionParameters;
    if (rule.eventType == EventType::stickMove)
    {
        auto stick = std::dynamic_pointer_cast<controls::Stick>(control);

        if (!stick)
            return actionParameters;

        auto xRange = stick->outputXRange();
        auto yRange = stick->outputYRange();
        auto zRange = stick->outputZRange();

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
        auto dataRole = fromActionParameterNameToItemDataRole(actParameter.first);
        if (!dataRole)
            continue;

        actionParameters.setArgument(
            dataRole.get(),
            fromActionParameterValueToVariant(
                dataRole.get(),
                actParameter.second));
    }

    return actionParameters;
}

boost::optional<Qn::ItemDataRole> Manager::fromActionParameterNameToItemDataRole(
    const QString& actionParamterName) const
{
    if (actionParamterName == config::kPresetIndexParameterName)
        return Qn::ItemDataRole::PtzPresetIndexRole;

    return boost::none;
}

QVariant Manager::fromActionParameterValueToVariant(
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
} // namespace io_device
} // namespace plugins
} // namespace client
} // namespace nx