#pragma once

#include <map>

#include "joystick_common.h"
#include "joystick_mapping.h"
#include "abstract_joystick_driver.h"

#include <utils/common/singleton.h>
#include <ui/workbench/workbench_context.h>
#include <ui/widgets/main_window.h>

namespace nx {
namespace joystick {

class Manager: public QObject, public Singleton<Manager>
{
    Q_OBJECT 
public:
    Manager(
        QApplication* m_application,
        QnMainWindow* mainWindow,
        QnWorkbenchContext* context);

    virtual ~Manager();
    void start();

signals:
    void joystickMove(const nx::joystick::State& state);

private:
    void loadDrivers();
    void loadMappings();
    void applyMappings(
        std::vector<JoystickPtr>& joysticks,
        std::multimap<QString, mapping::Rule> mappings);

    EventHandler makeEventHandler(mapping::Rule& rule);

    void addEventParametersToAction(
        const mapping::Rule& rule,
        const EventParameters& eventParameters,
        QnActionParameters* inOutActionParameters);

private:
    typedef std::unique_ptr<driver::AbstractJoystickDriver> DriverPtr;

    QApplication* m_application;
    QnMainWindow* m_mainWindow;
    QnWorkbenchContext* m_context;
    std::vector<DriverPtr> m_drivers;
    std::multimap<QString, mapping::Rule> m_mappings;

    mutable QnMutex m_mutex;
};

} // namespace joystick
} // namespace nx