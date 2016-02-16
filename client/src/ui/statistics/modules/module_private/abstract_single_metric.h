
#pragma once

class AbstractSingleMetric
{
public:
    AbstractSingleMetric() {}

    virtual ~AbstractSingleMetric() {}

    virtual bool significant() const = 0;

    virtual QString value() const = 0;

    virtual void reset() = 0;
};