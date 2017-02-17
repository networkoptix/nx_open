#pragma once

#include <map>

#include "joystick_common.h"
#include "joystick_mapping.h"
#include "joystick_config.h"

#include <plugins/io_device/joystick/drivers/abstract_joystick_driver.h>
#include <utils/common/singleton.h>
#include <ui/workbench/workbench_context.h>
#include <ui/widgets/main_window.h>

namespace nx {
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

    EventHandler makeEventHandler(const mapping::Rule& rule, controls::ControlPtr control);

    QnActionParameters createActionParameters(
        const mapping::Rule& rule,
        const controls::ControlPtr& control,
        const EventParameters& eventParameters);

    boost::optional<Qn::ItemDataRole> fromActionParamterNameToItemDataRole(
        const QString& actionParamterName) const;

    QVariant fromActionParamtereValueToVariant(
        Qn::ItemDataRole dataRole,
        const QString& actionParamterValue) const;

private:
    typedef std::unique_ptr<driver::AbstractJoystickDriver> DriverPtr;

    std::vector<DriverPtr> m_drivers;
    mapping::ConfigHolder m_configHolder;

    mutable QnMutex m_mutex;
};

} // namespace joystick
} // namespace nx