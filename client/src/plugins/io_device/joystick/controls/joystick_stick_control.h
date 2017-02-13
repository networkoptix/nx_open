#pragma once

#include "base_joystick_control.h"

namespace nx {
namespace joystick {
namespace controls {

typedef nx::joystick::State::size_type AxisIndex;
const AxisIndex kXindex = 0;
const AxisIndex kYindex = 1;
const AxisIndex kZindex = 2;
const AxisIndex kRxIndex = 3;
const AxisIndex kRyIndex = 4;
const AxisIndex kRzIndex = 5;

class Stick: public BaseControl
{

public:
    Stick();

    virtual LimitsVector getOutputLimits() const;
    virtual void setOutputLimits(LimitsVector outputLimits);

    virtual nx::joystick::StateAtom getStickX() const;
    virtual nx::joystick::StateAtom getStickY() const;
    virtual nx::joystick::StateAtom getStickZ() const;
    virtual nx::joystick::StateAtom getStickRx() const;
    virtual nx::joystick::StateAtom getStickRy() const;
    virtual nx::joystick::StateAtom getStickRz() const;

    virtual Limits getStickXLimits() const;
    virtual void setStickXLimits(nx::joystick::StateAtom min, nx::joystick::StateAtom max);
    virtual void setStickXLimits(Limits limits);

    virtual Limits getStickYLimits() const;
    virtual void setStickYLimits(nx::joystick::StateAtom min, nx::joystick::StateAtom max);
    virtual void setStickYLimits(Limits limits);

    virtual Limits getStickZLimits() const;
    virtual void setStickZLimits(nx::joystick::StateAtom min, nx::joystick::StateAtom max);
    virtual void setStickZLimits(Limits limits);

    virtual Limits getStickRxLimits() const;
    virtual void setStickRxLimits(nx::joystick::StateAtom min, nx::joystick::StateAtom max);
    virtual void setStickRxLimits(Limits limits);

    virtual Limits getStickRyLimits() const;
    virtual void setStickRyLimits(nx::joystick::StateAtom min, nx::joystick::StateAtom max);
    virtual void setStickRyLimits(Limits limits);

    virtual Limits getStickRzLimits() const;
    virtual void setStickRzLimits(nx::joystick::StateAtom min, nx::joystick::StateAtom max);
    virtual void setStickRzLimits(Limits limits);

    virtual std::vector<Limits> getLimits() const;
    virtual void setLimits(std::vector<Limits> limits);

protected:
    virtual bool isEventTypeSupported(nx::joystick::EventType eventType) const override;
    virtual EventSet checkForEventsUnsafe() const override;
    
    virtual nx::joystick::State fromRawToNormalized(
        const nx::joystick::State& raw) const override;

    virtual nx::joystick::State fromNormalizedToRaw(
        const nx::joystick::State& normalized) const override;

private:
    enum class StickArea
    {
        upArea,
        downArea,
        leftArea,
        rightArea
    };

private:
    nx::joystick::StateAtom distance(
        const nx::joystick::State& position1,
        const nx::joystick::State& position2) const;

    nx::joystick::StateAtom translatePoint(
        nx::joystick::StateAtom point,
        const Limits& originalBounds,
        const Limits& targetBounds) const;

    bool isZeroPosition(const nx::joystick::State& position) const;
    bool hasStickBeenPushedToArea(StickArea area) const;

private:
    State m_previousPosition;
    StateAtom m_threshold;
    LimitsVector m_limits;
    LimitsVector m_outputLimits;
};

typedef std::shared_ptr<Stick> StickPtr;

} // namespace controls
} // namespace joystick
} // namespace nx

