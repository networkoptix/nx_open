#pragma once

#include "base_joystick_control.h"

namespace nx {
namespace client {
namespace plugins {
namespace io_device {
namespace joystick {
namespace controls {

enum DegreeOfFreedom
{
    x = 0,
    y,
    z,
    rX,
    rY,
    rZ
};

class Stick: public BaseControl
{
    typedef BaseControl base_type;
public:
    Stick();

    virtual StateElement x() const;
    virtual StateElement y() const;
    virtual StateElement z() const;
    virtual StateElement rX() const;
    virtual StateElement rY() const;
    virtual StateElement rZ() const;

    virtual Ranges ranges() const;
    virtual void setRanges(Ranges range);

    virtual Ranges outputRanges() const;
    virtual void setOutputRanges(Ranges outputLimits);

    virtual std::vector<bool> inversions() const;
    virtual void setInversions(const std::vector<bool>& inverions);

    virtual void invertX(bool invert);
    virtual bool isXInverted() const;
    
    virtual void invertY(bool invert);
    virtual bool isYInverted() const;

    virtual void invertZ(bool invert);
    virtual bool isZInverted() const;

    virtual void invertRx(bool invert);
    virtual bool isRxInverted() const;

    virtual void invertRy(bool invert);
    virtual bool isRyInverted() const;

    virtual void invertRz(bool invert);
    virtual bool isRzInverted() const;

    virtual Range xRange() const;
    virtual void setStickXRange(
        StateElement min,
        StateElement max);
    virtual void setStickXRange(const Range& range);

    virtual Range yRange() const;
    virtual void setStickYRange(
        StateElement min,
        StateElement max);
    virtual void setStickYRange(const Range& range);

    virtual Range zRange() const;
    virtual void setStickZRange(
        StateElement min,
        StateElement max);
    virtual void setStickZRange(const Range& range);

    virtual Range rXRange() const;
    virtual void setStickRxRange(
        StateElement min,
        StateElement max);
    virtual void setStickRxRange(const Range& range);

    virtual Range rYRange() const;
    virtual void setStickRyRange(
        StateElement min,
        StateElement max);
    virtual void setStickRyRange(const Range& range);

    virtual Range rZRange() const;
    virtual void setStickRzRange(
        StateElement min,
        StateElement max);
    virtual void setStickRzRange(const Range& range);

    virtual Range outputXRange() const;
    virtual void setStickOutputXRange(
        StateElement min,
        StateElement max);
    virtual void setStickOutputXRange(const Range& range);

    virtual Range outputYRange() const;
    virtual void setStickOutputYRange(
        StateElement min,
        StateElement max);
    virtual void setStickOutputYRange(const Range& range);

    virtual Range outputZRange() const;
    virtual void setStickOutputZRange(
        StateElement min,
        StateElement max);
    virtual void setStickOutputZRange(const Range& range);

    virtual Range outputRxRange() const;
    virtual void setStickOutputRxRange(
        StateElement min,
        StateElement max);
    virtual void setStickOutputRxRange(const Range& range);

    virtual Range outputRyRange() const;
    virtual void setStickOutputRyRange(
        StateElement min,
        StateElement max);
    virtual void setStickOutputRyRange(const Range& range);

    virtual Range outputRzRange() const;
    virtual void setStickOutputRzRange(
        StateElement min,
        StateElement max);
    virtual void setStickOutputRzRange(const Range& range);

    virtual void applyOverride(
        const QString overrideName,
        const QString& overrideValue) override;

protected:
    virtual bool isEventTypeSupported(EventType eventType) const override;
    virtual EventSet checkForEventsUnsafe() const override;
    virtual void setStateUnsafe(const State& state) override;
    virtual State fromRawToNormalized(
        const State& raw) const override;

    virtual State fromNormalizedToRaw(
        const State& normalized) const override;

private:
    StateElement distance(
    const State& position1,
    const State& position2) const;

    StateElement translatePoint(
        StateElement point,
        const Range& originalBounds,
        const Range& targetBounds) const;

    bool isZeroPosition(const State& position) const;

    void fixState(State* inOutState);

private:
    StateElement m_threshold;
    State m_previousPosition;
    Ranges m_ranges;
    Ranges m_outputRanges;
    std::vector<bool> m_inversions;
};

typedef std::shared_ptr<Stick> StickPtr;

} // namespace controls
} // namespace joystick
} // namespace io_device
} // namespace plugins
} // namespace client
} // namespace nx

