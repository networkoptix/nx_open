#pragma once

#include "abstract_joystick.h"

#include <plugins/io_device/joystick/drivers/abstract_joystick_driver.h>
#include <plugins/io_device/joystick/controls/joystick_button_control.h>
#include <plugins/io_device/joystick/controls/joystick_stick_control.h>
#include <plugins/io_device/joystick/controls/joystick_led_control.h>


namespace nx {
namespace client {
namespace plugins {
namespace io_device {
namespace joystick {

class GenericJoystick: public AbstractJoystick
{
public:
    GenericJoystick();
    virtual ~GenericJoystick() override;

    virtual QString id() const;
    virtual void setId(const QString& id);

    virtual QString model() const override;
    virtual void setModel(const QString& model) override;

    virtual QString vendor() const override;
    virtual void setVendor(const QString& vendor) override;

    virtual controls::ControlPtr controlById(const QString& controlId) const override;

    virtual std::vector<controls::ControlPtr> controls() const override;
    virtual void setControls(std::vector<controls::ControlPtr> joystickControls) override;

    virtual std::vector<controls::ButtonPtr> getButtons();
    virtual void setButtons(std::vector<controls::ButtonPtr> joystickButtons);

    virtual std::vector<controls::StickPtr> getSticks();
    virtual void setSticks(std::vector<controls::StickPtr> joystickSticks);

    virtual std::vector<controls::LedPtr> getLeds();
    virtual void setLeds(std::vector<controls::LedPtr> joystickLeds);

    virtual bool setControlState(const QString& controlId, const State& state) override;
    virtual void notifyControlStateChanged(const QString& controlId, const State& state) override;

    virtual driver::AbstractJoystickDriver* driver() const override;
    virtual void setDriver(driver::AbstractJoystickDriver* driver) override;

private:
    typedef std::map<QString, controls::ControlPtr> ControlMap;

private:
    // All the functions below use std::dynamic_pointer_cast which doesn't seem to be very efficient.
    template<typename ControlType>
    std::vector<std::shared_ptr<ControlType>> filterControls(const ControlMap& controlMap) const;

    template<typename ControlType>
    void removeControls(ControlMap& controlMap) const;

    template<typename ControlType>
    void addControls(
        const std::vector<std::shared_ptr<ControlType>>& controlsToAdd,
        ControlMap& controlMap) const;

private:
    QString m_id;
    QString m_model;
    QString m_vendor;
    
    ControlMap m_controls;

    driver::AbstractJoystickDriver* m_driver;
    mutable QnMutex m_mutex;
};

template<typename ControlType>
std::vector<std::shared_ptr<ControlType>> GenericJoystick::filterControls(const ControlMap& controlMap) const
{
    std::vector<std::shared_ptr<ControlType>> filtered;

    for (const auto& item: controlMap)
    {
        if (auto casted = std::dynamic_pointer_cast<ControlType>(item.second))
            filtered.push_back(casted);
    }

    return filtered;
}

template<typename ControlType>
void GenericJoystick::removeControls(ControlMap& controlMap) const
{
    for (auto itr = controlMap.begin(); itr != controlMap.end();)
    {
        if (std::dynamic_pointer_cast<ControlType>(itr->second))
            itr = controlMap.erase(itr);
        else
            ++itr;
    }
}

template<typename ControlType>
void GenericJoystick::addControls(
    const std::vector<std::shared_ptr<ControlType>>& controlsToAdd,
    ControlMap& controlMap) const
{
    for (const auto& control: controlsToAdd)
        controlMap[control->getId()] = std::dynamic_pointer_cast<ControlType>(control);
}

} // namespace joystick
} // namespace io_device
} // namespace plugins
} // namespace client
} // namespace nx
