
#pragma once

#include <QtCore/QObject>

namespace statistics
{
    enum MetricLifetime
    {
        kSessionLifetime
        , kInstanceLifetime
    };

    enum MetricType
    {
        kTriggeredCount

        , kActivationsCount
        , kDeactivationsCount
        , kActiveTime
        , kInactiveTime
    };

    class QnAbstractStatisticsMetric : public QObject
    {
        Q_OBJECT

        Q_PROPERTY(bool isTurnedOn READ isTurnedOn WRITE setTurnedOn NOTIFY turnedOnChanged)
    public:
        QnAbstractStatisticsMetric(const QString &alias);

        virtual ~QnAbstractStatisticsMetric();

    public:
        virtual QString serialize() = 0;

        // Called before serialization, to fix result of metric
        // (for prolonged metrics like kActiveTime)
        virtual void fixValue();

    public:

        void setTurnedOn(bool turnOn);

        bool isTurnedOn();

        QString alias() const;

    signals:
        void turnedOnChanged();

    private:
        QString m_alias;
        bool m_isTurnedOn;
    };
}

namespace statistics
{
    namespace details
    {
        template<typename ValueType>
        class QnTypedAbstractStatisticsMetric : public statistics::QnAbstractStatisticsMetric
        {
        public:
            QnTypedAbstractStatisticsMetric(const QString &fieldName
                , const ValueType &initValue = ValueType());

            virtual ~QnTypedAbstractStatisticsMetric();

            const ValueType &value() const;

            void setValue(const ValueType &newValue);

        public:
            // Overrides

            virtual QString serialize() override;

        private:
            ValueType m_value;
        };
    }
}

// Implementation of QnTypedAbstractStatisticsMetric
namespace statistics
{
    namespace details
    {
        template<typename ValueType>
        QnTypedAbstractStatisticsMetric<ValueType>::QnTypedAbstractStatisticsMetric(const QString &fieldName
            , const ValueType &initValue)
            : QnAbstractStatisticsMetric(fieldName)
            , m_value(initValue)
        {
        }

        template<typename ValueType>
        QnTypedAbstractStatisticsMetric<ValueType>::~QnTypedAbstractStatisticsMetric()
        {
        }

        template<typename ValueType>
        const ValueType &QnTypedAbstractStatisticsMetric<ValueType>::value() const
        {
            return m_value;
        }

        template<typename ValueType>
        void QnTypedAbstractStatisticsMetric<ValueType>::setValue(const ValueType &newValue)
        {
            m_value = newValue;
        }

        template<typename ValueType>
        QString QnTypedAbstractStatisticsMetric<ValueType>::serialize()
        {
            this->fixValue();
            // TODO: #ynikitenkov Add fusion serialization
            return QString();
        }
    }
}