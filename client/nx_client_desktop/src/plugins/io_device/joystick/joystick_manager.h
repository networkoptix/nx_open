#pragma once

#include <map>

#include "joystick_common.h"
#include "joystick_config.h"
#include "joystick_config_holder.h"

#include <plugins/io_device/joystick/drivers/abstract_joystick_driver.h>
#include <nx/utils/singleton.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/widgets/main_window.h>

namespace nx {
namespace client {
namespace plugins {
namespace io_device {
namespace joystick {

class Manager:
    public QObject,
    public Singleton<Manager>,
    public QnWorkbenchContextAware
{
    Q_OBJECT
public:
    Manager(QnWorkbenchContext* context);
    virtual ~Manager();
    void start();

    void notifyHardwareConfigurationChanged();

private:
    void loadDrivers();
    bool loadMappings();
    void applyMappings(std::vector<JoystickPtr>& joysticks);
    void applyMappingsAndCaptureJoysticks();

    EventHandler makeEventHandler(const config::Rule& rule, controls::ControlPtr control);

    nx::client::desktop::ui::action::Parameters createActionParameters(
        const config::Rule& rule,
        const controls::ControlPtr& control,
        const EventParameters& eventParameters);

    boost::optional<Qn::ItemDataRole> fromActionParameterNameToItemDataRole(
        const QString& actionParamterName) const;

    QVariant fromActionParameterValueToVariant(
        Qn::ItemDataRole dataRole,
        const QString& actionParamterValue) const;

private:
    typedef std::unique_ptr<driver::AbstractJoystickDriver> DriverPtr;

    std::vector<DriverPtr> m_drivers;
    config::ConfigHolder m_configHolder;

    mutable QnMutex m_mutex;
};

} // namespace joystick
} // namespace io_device
} // namespace plugins
} // namespace client
} // namespace nx