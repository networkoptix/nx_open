#include "joystick_manager.h"
#include "abstract_joystick.h"

#if defined(Q_OS_WIN)
#include "mm_joystick_driver_win.h"
#include "joystick_event_filter_win.h"
#endif

#include <ui/actions/action_manager.h>

namespace nx {
namespace joystick {

Manager::Manager(
    QApplication* application,
    QnMainWindow* mainWindow,
    QnWorkbenchContext* context):

    m_application(application),
    m_mainWindow(mainWindow),

    m_context(context)
{
}

Manager::~Manager()
{
}

void Manager::start()
{
    loadDrivers();
    loadMappings();

    std::vector<nx::joystick::JoystickPtr> joysticks;
    for (const auto& drv: m_drivers)
    {
        auto joysticks = drv->enumerateJoysticks();
        applyMappings(joysticks, m_mappings);
        
        for(auto& joy: joysticks)
            drv->captureJoystick(joy);
    }
}

void Manager::loadDrivers()
{
#if defined(Q_OS_WIN)
    auto hwndId = reinterpret_cast<HWND>(m_mainWindow->winId());
    m_drivers.emplace_back(new driver::MmWinDriver(hwndId));

    m_application->installNativeEventFilter(new nx::joystick::JoystickEventFilter(hwndId));
#endif
}

void Manager::loadMappings()
{
    m_mappings.emplace(
        lit("stick:0"), 
        mapping::Rule(
            lit("stick:0"),
            EventType::stickNonZero,
            QnActions::PtzStartContinuousMove,
            QnActionParameters()));

    m_mappings.emplace(
        lit("stick:0"),
        mapping::Rule(
            lit("stick:0"),
            EventType::stickZero,
            QnActions::PtzStartContinuousMove,
            QnActionParameters()));
}


void Manager::applyMappings(
    std::vector<JoystickPtr>& joysticks,
    std::multimap<QString, mapping::Rule> mappings)
{
    for (auto& joy: joysticks)
    {
        auto controls = joy->getControls();
        for (auto& control: controls)
        {
            auto rules = m_mappings.equal_range(control->getId());
            if (rules.first == rules.second)
                continue;

            for (auto itr = rules.first; itr != rules.second; ++itr)
            {
                auto rule = itr->second;
                bool ruleHasBeenAdded = control->addEventHandler(
                        rule.triggerCondition, 
                        makeEventHandler(rule));
                
                if (!ruleHasBeenAdded)
                    qDebug() << "Can not add rule for control" << control->getId();
            }
        }
    }
}

nx::joystick::EventHandler Manager::makeEventHandler(mapping::Rule& rule)
{
    auto handler = 
        [rule, this](EventType eventType, EventParameters eventParameters)
        {
            if (!m_context)
                return;

            auto actionParameters = rule.actionParameters;
            addEventParametersToAction(
                rule,
                eventParameters,
                &actionParameters);

            if (eventType == EventType::stickNonZero || eventType == EventType::stickZero)
            {
                qDebug() << "stick non zero!!!!";
                QString stateStr = lit("STATE: ");
                for (const auto& atom: eventParameters.state)
                {
                    stateStr += QString::number(atom) + lit(", ");
                }
                qDebug() << stateStr;

                emit joystickMove(eventParameters.state);
            }
            else
            {
                m_context->menu()->trigger(
                    rule.actionType,
                    actionParameters);
            }

            
        };

    return handler;
}

void Manager::addEventParametersToAction(
    const mapping::Rule& rule,
    const EventParameters& eventParameters,
    QnActionParameters* inOutActionParameters)
{
    // TODO: #dmishin implement it
}

} // namespace joystick
} // namespace nx

