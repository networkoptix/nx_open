
#pragma once

#include <nx/utils/singleton.h>
#include <statistics/abstract_statistics_module.h>

class QnControlsStatisticsModule : public QnAbstractStatisticsModule
{
    Q_OBJECT

public:
    explicit QnControlsStatisticsModule(QObject* parent = nullptr);
    virtual ~QnControlsStatisticsModule();

    QnStatisticValuesHash values() const override;

    virtual void reset() override;

    template<typename ButtonType>
    void registerButton(const QString& alias, ButtonType* button);

    template<typename SliderType>
    void registerSlider(const QString& alias, SliderType* slider);

    void registerClick(const QString& alias);

private:
    typedef QHash<QString, int> ClicksCountHash;

    ClicksCountHash m_clicksCount;
};

template<typename ButtonType>
void QnControlsStatisticsModule::registerButton(const QString &alias
    , ButtonType *button)
{
    const QPointer<QnControlsStatisticsModule> guard(this);
    const auto &nonEmptyAlias = (alias.isEmpty()
        ? lit("undefined_button") : alias);
    const auto handler = [this, guard, nonEmptyAlias]()
    {
        if (guard)
            registerClick(nonEmptyAlias);
    };

    connect(button, &ButtonType::pressed, this, handler);
}

template<typename SliderType>
void QnControlsStatisticsModule::registerSlider(const QString &alias
    , SliderType *slider)
{
    const QPointer<QnControlsStatisticsModule> guard(this);
    const auto &nonEmptyAlias = (alias.isEmpty()
        ? lit("undefined_slider") : alias);
    const auto handler = [this, guard, nonEmptyAlias]()
    {
        if (guard)
            registerClick(nonEmptyAlias);
    };

    connect(slider, &SliderType::sliderPressed, this, handler);
}
