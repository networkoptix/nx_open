#pragma once

#include "value_item.h"

namespace nx::vms::server::interactive_settings::components {

class NumericValueItem: public ValueItem
{
public:
    using ValueItem::ValueItem;

    virtual QJsonValue normalizedValue(const QJsonValue& value) const override;
    virtual QJsonValue fallbackDefaultValue() const override;
};

class IntegerValueItem: public NumericValueItem
{
    Q_OBJECT
    Q_PROPERTY(int minValue READ minValue WRITE setMinValue NOTIFY minValueChanged FINAL)
    Q_PROPERTY(int maxValue READ maxValue WRITE setMaxValue NOTIFY maxValueChanged FINAL)

public:
    using NumericValueItem::NumericValueItem;

    int minValue() const { return m_minValue; }
    void setMinValue(int minValue);

    int maxValue() const { return m_maxValue; }
    void setMaxValue(int maxValue);

    virtual QJsonValue normalizedValue(const QJsonValue& value) const override;
    virtual QJsonValue fallbackDefaultValue() const override;
    virtual QJsonObject serializeModel() const override;

signals:
    void minValueChanged();
    void maxValueChanged();

protected:
    int m_minValue = std::numeric_limits<int>::min();
    int m_maxValue = std::numeric_limits<int>::max();
};

class RealValueItem: public NumericValueItem
{
    Q_OBJECT
    Q_PROPERTY(double minValue READ minValue WRITE setMinValue NOTIFY minValueChanged FINAL)
    Q_PROPERTY(double maxValue READ maxValue WRITE setMaxValue NOTIFY maxValueChanged FINAL)

public:
    using NumericValueItem::NumericValueItem;

    double minValue() const { return m_minValue; }
    void setMinValue(double minValue);

    double maxValue() const { return m_maxValue; }
    void setMaxValue(double maxValue);

    virtual QJsonValue normalizedValue(const QJsonValue& value) const override;
    virtual QJsonValue fallbackDefaultValue() const override;
    virtual QJsonObject serializeModel() const override;

signals:
    void minValueChanged();
    void maxValueChanged();

protected:
    double m_minValue = std::numeric_limits<double>::min();
    double m_maxValue = std::numeric_limits<double>::max();

    friend QJsonValue normalizedValue(ValueItem* item, const QJsonValue& value);
};

} // namespace nx::vms::server::interactive_settings::components
