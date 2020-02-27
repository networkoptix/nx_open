#pragma once

#include "value_item.h"

namespace nx::vms::server::interactive_settings::components {

class IntegerNumberItem: public ValueItem
{
    Q_OBJECT
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged FINAL)
    Q_PROPERTY(int minValue READ minValue WRITE setMinValue NOTIFY minValueChanged FINAL)
    Q_PROPERTY(int maxValue READ maxValue WRITE setMaxValue NOTIFY maxValueChanged FINAL)

    using base_type = ValueItem;

public:
    using ValueItem::ValueItem;

    int value() const
    {
        return ValueItem::value().toInt();
    }

    void setValue(int value);
    virtual void setValue(const QVariant& value) override;

    int minValue() const
    {
        return m_minValue;
    }

    void setMinValue(int minValue);

    int maxValue() const
    {
        return m_maxValue;
    }

    void setMaxValue(int maxValue);

    virtual QJsonObject serialize() const override;

signals:
    void minValueChanged();
    void maxValueChanged();

protected:
    int m_minValue = std::numeric_limits<int>::min();
    int m_maxValue = std::numeric_limits<int>::max();
};

class RealNumberItem: public ValueItem
{
    Q_OBJECT
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged FINAL)
    Q_PROPERTY(double minValue READ minValue WRITE setMinValue NOTIFY minValueChanged FINAL)
    Q_PROPERTY(double maxValue READ maxValue WRITE setMaxValue NOTIFY maxValueChanged FINAL)

    using base_type = ValueItem;

public:
    using ValueItem::ValueItem;

    double value() const
    {
        return ValueItem::value().toDouble();
    }

    void setValue(double value);
    virtual void setValue(const QVariant& value) override;

    double minValue() const
    {
        return m_minValue;
    }

    void setMinValue(double minValue);

    double maxValue() const
    {
        return m_maxValue;
    }

    void setMaxValue(double maxValue);

    virtual QJsonObject serialize() const override;

signals:
    void minValueChanged();
    void maxValueChanged();

protected:
    double m_minValue = std::numeric_limits<double>::min();
    double m_maxValue = std::numeric_limits<double>::max();
};

} // namespace nx::vms::server::interactive_settings::components
