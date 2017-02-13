#include "generic_joystick.h"

namespace nx {
namespace joystick {

GenericJoystick::GenericJoystick()
{

}

GenericJoystick::~GenericJoystick()
{

}

QString GenericJoystick::getId() const
{
    QnMutexLocker lock(&m_mutex);
    return m_id;
}

void GenericJoystick::setId(const QString& id)
{
    QnMutexLocker lock(&m_mutex);
    m_id = id;
}

QString GenericJoystick::getModel() const
{
    QnMutexLocker lock(&m_mutex);
    return m_model;
}

void GenericJoystick::setModel(const QString& model)
{
    QnMutexLocker lock(&m_mutex);
    m_model = model;
}

QString GenericJoystick::getVendor() const
{
    QnMutexLocker lock(&m_mutex);
    return m_vendor;
}

void GenericJoystick::setVendor(const QString& vendor)
{
    QnMutexLocker lock(&m_mutex);
    m_vendor = vendor;
}

controls::ControlPtr GenericJoystick::getControlById(const QString& controlId) const 
{
    QnMutexLocker lock(&m_mutex);
    if (m_controls.find(controlId) == m_controls.end())
        return nullptr;

    return m_controls.at(controlId);
}

std::vector<controls::ControlPtr> GenericJoystick::getControls() const
{
    QnMutexLocker lock(&m_mutex);
    std::vector<controls::ControlPtr> result;

    for (const auto& item: m_controls)
        result.push_back(item.second);

    return result;
}

void GenericJoystick::setControls(std::vector<controls::ControlPtr> joystickControls)
{
    QnMutexLocker lock(&m_mutex);
    m_controls.clear();
    for (const auto& control: joystickControls)
        m_controls[control->getId()] = control;
}

std::vector<controls::ButtonPtr> GenericJoystick::getButtons()
{
    QnMutexLocker lock(&m_mutex);
    return filterControls<controls::Button>(m_controls);
}

void GenericJoystick::setButtons(std::vector<controls::ButtonPtr> joystickButtons)
{
    QnMutexLocker lock(&m_mutex);
    removeControls<controls::Button>(m_controls);
    addControls(joystickButtons, m_controls);
}

std::vector<controls::StickPtr> GenericJoystick::getSticks()
{
    QnMutexLocker lock(&m_mutex);
    return filterControls<controls::Stick>(m_controls);
}

void GenericJoystick::setSticks(std::vector<controls::StickPtr> joystickSticks)
{
    QnMutexLocker lock(&m_mutex);
    removeControls<controls::Stick>(m_controls);
    addControls(joystickSticks, m_controls);
}

std::vector<controls::LedPtr> GenericJoystick::getLeds()
{
    QnMutexLocker lock(&m_mutex);
    return filterControls<controls::Led>(m_controls);
}

void GenericJoystick::setLeds(std::vector<controls::LedPtr> joystickLeds)
{
    QnMutexLocker lock(&m_mutex);
    removeControls<controls::Led>(m_controls);
    addControls(joystickLeds, m_controls);
}

std::vector<controls::WheelPtr> GenericJoystick::getWheels()
{
    QnMutexLocker lock(&m_mutex);
    return filterControls<controls::Wheel>(m_controls);
}

void GenericJoystick::setWheels(std::vector<controls::WheelPtr> joystickWheels)
{
    QnMutexLocker lock(&m_mutex);
    removeControls<controls::Wheel>(m_controls);
    addControls<controls::Wheel>(joystickWheels, m_controls);
}

std::vector<controls::SliderPtr> GenericJoystick::getSliders()
{
    QnMutexLocker lock(&m_mutex);
    return filterControls<controls::Slider>(m_controls);
}

void GenericJoystick::setSliders(std::vector<controls::SliderPtr> joystickSliders)
{
    QnMutexLocker lock(&m_mutex);
    removeControls<controls::Slider>(m_controls);
    addControls(joystickSliders, m_controls);
}

bool GenericJoystick::setControlState(const QString& controlId, const State& state)
{
    QnMutexLocker lock(&m_mutex);
    if (!m_driver)
        return false;

    return m_driver->setControlState(controlId, state);
}

void GenericJoystick::updateControlStateWithRawValue(
    const QString& controlId,
    const State& state)
{
    auto control = getControlById(controlId);
    control->updateStateWithRawValue(state);
}

driver::AbstractJoystickDriver* GenericJoystick::getDriver() const
{
    QnMutexLocker lock(&m_mutex);
    return m_driver;
}

void GenericJoystick::setDriver(driver::AbstractJoystickDriver* driver)
{
    QnMutexLocker lock(&m_mutex);
    m_driver = driver;
}

} // namespace joystick
} // namespace nx