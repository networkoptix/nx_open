
#pragma once

#include <utils/common/singleton.h>
#include <statistics/abstract_statistics_module.h>

class QnButtonsStatisticsModule : public QnAbstractStatisticsModule
    , public Singleton<QnButtonsStatisticsModule>
{
    Q_OBJECT

public:
    QnButtonsStatisticsModule();

    virtual ~QnButtonsStatisticsModule();

    QnStatisticValuesHash values() const override;

    virtual void reset() override;

    template<typename ButtonType>
    void registerButton(const QString &alias
        , ButtonType *button);

private:
    void onButtonPressed(const QString &alias);

private:
    typedef QHash<QString, int> ClicksCountHash;

    ClicksCountHash m_clicksCount;
};

template<typename ButtonType>
void QnButtonsStatisticsModule::registerButton(const QString &alias
    , ButtonType *button)
{
    const QPointer<QnButtonsStatisticsModule> guard(this);
    const auto &nonEmptyAlias = (alias.isEmpty()
        ? lit("undefined_button") : alias);
    const auto handler = [this, guard, nonEmptyAlias]()
    {
        if (guard)
            onButtonPressed(nonEmptyAlias);
    };

    connect(button, &ButtonType::pressed, this, handler);
}

#define qnButtonsStatisticsModule QnButtonsStatisticsModule::instance()