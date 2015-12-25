
#pragma once

class QnAbstractStatisticsMetric
{
public:
    QnAbstractStatisticsMetric(const QString &fieldName);

    virtual QString serialize() const = 0;

private:
    QString m_fieldName;
};

//

template<typename ValueType>
class QnTypedAbstractStatisticsMetric : public QnAbstractStatisticsMetric
{
public:
    QnTypedAbstractStatisticsMetric(const QString &fieldName);

    virtual ~QnTypedAbstractStatisticsMetric();

    const ValueType &value() const;

    void setValue(const ValueType &newValue);

public:
    // Overrides

    virtual QString serialize() const override;

private:
    ValueType m_value;
};

// Implementation of template classes

template<typename ValueType>
QnTypedAbstractStatisticsMetric::QnTypedAbstractStatisticsMetric(const QString &fieldName)
    : QnAbstractStatisticsMetric(fieldName)
    , m_value()
{
}

template<typename ValueType>
QnTypedAbstractStatisticsMetric::~QnTypedAbstractStatisticsMetric()
{
}