
#pragma once

#include <QtCore/QString>

// @brief Represents abstract class for single metric
class QnAbstractMetric
{
public:
    QnAbstractMetric();

    virtual ~QnAbstractMetric();

    // @brief Non-significant values will be reject
    // Function should be overridden and return 'false'
    // value in case you don't want to send it to
    // statistics server
    virtual bool isSignificant() const;

    // @brief Returns value of the metric
    virtual QString value() const = 0;

    // @brief Resets value of metric
    virtual void reset() = 0;
};
