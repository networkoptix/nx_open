#include "widgets.h"

#include <QtGui/QBoxLayout>
#include <QtGui/QCheckBox>
#include <QtGui/QGroupBox>
#include <QtGui/QPushButton>
#include <QtGui/QRadioButton>

#include "settings.h"

SettingsSlider::SettingsSlider(Qt::Orientation orientation, QWidget *parent)
    : QSlider(orientation, parent)
{
}

void SettingsSlider::keyReleaseEvent(QKeyEvent *event)
{
    QSlider::keyReleaseEvent(event);

    emit onKeyReleased();
}
//==============================================

CLAbstractSettingsWidget::CLAbstractSettingsWidget(QnResourcePtr resource, const QnParam &param)
    : QObject(),
      m_resource(resource),
      m_param(param),
      m_widget(0)
{
}

CLAbstractSettingsWidget::~CLAbstractSettingsWidget()
{
}

QnResourcePtr CLAbstractSettingsWidget::resource() const
{
    return m_resource;
}

const QnParam &CLAbstractSettingsWidget::param() const
{
    return m_param;
}

QString CLAbstractSettingsWidget::group() const
{
    return m_param.group();
}

QString CLAbstractSettingsWidget::subGroup() const
{
    return m_param.subgroup();
}

QWidget *CLAbstractSettingsWidget::widget() const
{
    if (!m_widget)
        m_widget = const_cast<CLAbstractSettingsWidget *>(this)->createWidget();
    return m_widget;
}

void CLAbstractSettingsWidget::setParamValue(const QVariant &value)
{
    m_param.setValue(value);
    Q_EMIT setParam(m_param.name(), value);
}

//==============================================

SettingsOnOffWidget::SettingsOnOffWidget(QnResourcePtr dev, const QnParam &param)
    : CLAbstractSettingsWidget(dev, param),
      m_checkBox(0)
{
}

QWidget *SettingsOnOffWidget::createWidget()
{
    Q_ASSERT_X(param().possibleValues().count() == 2, "SettingsOnOffWidget::createWidget()", "param.possible_values.count()!=2 !!!!");

    m_checkBox = new QCheckBox(param().name());

    updateParam(param().value());

    connect(m_checkBox, SIGNAL(toggled(bool)), this, SLOT(toggled(bool)));

    return m_checkBox;
}

void SettingsOnOffWidget::updateParam(const QVariant &value)
{
    m_checkBox->setChecked(value == param().possibleValues().front()); // might emit toggled()
}

void SettingsOnOffWidget::toggled(bool checked)
{
    setParamValue(checked ? param().possibleValues().front() : param().possibleValues().back());
}

//==============================================
SettingsMinMaxStepWidget::SettingsMinMaxStepWidget(QnResourcePtr dev, const QnParam &param)
    : CLAbstractSettingsWidget(dev, param),
      m_groupBox(0), m_slider(0)
{
}

QWidget *SettingsMinMaxStepWidget::createWidget()
{
    m_groupBox = new QGroupBox(param().name());
    //m_groupBox->setFont(QFont("Courier New", 11));

    m_slider = new SettingsSlider(Qt::Horizontal, m_groupBox);
    m_slider->setMinimumWidth(130);
    m_slider->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_slider->setRange(param().minValue(), param().maxValue());
    m_slider->setValue(param().value().toInt());

    connect(m_slider, SIGNAL(sliderReleased()), this, SLOT(onValueChanged()));
    connect(m_slider, SIGNAL(onKeyReleased()), this, SLOT(onValChanged()));
    connect(m_slider, SIGNAL(valueChanged(int)), this, SLOT(onValueChanged(int)));

    QVBoxLayout *layout = new QVBoxLayout(m_groupBox);
    layout->addWidget(m_slider);

    onValueChanged(param().value().toInt());

    return m_groupBox;
}

void SettingsMinMaxStepWidget::onValueChanged()
{
    setParamValue(m_slider->value());
}

void SettingsMinMaxStepWidget::onValueChanged(int value)
{
    m_groupBox->setTitle(param().name() + QLatin1Char('(') + QString::number(value) + QLatin1Char(')'));
}

void SettingsMinMaxStepWidget::updateParam(const QVariant &value)
{
    m_slider->setValue(value.toInt());
}

//==============================================
SettingsEnumerationWidget::SettingsEnumerationWidget(QnResourcePtr dev, const QnParam &param)
    : CLAbstractSettingsWidget(dev, param)
{
}

QWidget *SettingsEnumerationWidget::createWidget()
{
    QGroupBox *groupBox = new QGroupBox(param().name());
    groupBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    groupBox->setMinimumWidth(140);

    QVBoxLayout *layout = new QVBoxLayout(groupBox);
    foreach (const QVariant &val, param().uiPossibleValues()) {
        const QString name = val.toString();

        QRadioButton *button = new QRadioButton(name, groupBox);
        button->setObjectName(name);
        button->setFont(settings_font);
        button->setChecked(name == param().value().toString());

        connect(button, SIGNAL(clicked()), this, SLOT(onClicked()));

        layout->addWidget(button);

        m_radioButtons.append(button);
    }

    return groupBox;
}

void SettingsEnumerationWidget::onClicked()
{
    setParamValue(sender()->objectName());
}

void SettingsEnumerationWidget::updateParam(const QVariant &value)
{
    if (QRadioButton *button = getButtonByname(value.toString()))
        button->setChecked(true);
}

QRadioButton *SettingsEnumerationWidget::getButtonByname(const QString &name) const
{
    foreach (QRadioButton *button, m_radioButtons)
    {
        if (button->objectName() == name)
            return button;
    }

    return 0;
}

//==================================================
SettingsButtonWidget::SettingsButtonWidget(QnResourcePtr dev, const QnParam &param)
    : CLAbstractSettingsWidget(dev, param)
{
}

QWidget *SettingsButtonWidget::createWidget()
{
    QPushButton *button = new QPushButton(param().name());
    button->setFocusPolicy(Qt::NoFocus);

    connect(button, SIGNAL(released()), this, SLOT(onClicked()));

    return button;
}

void SettingsButtonWidget::onClicked()
{
    setParamValue("");
}

void SettingsButtonWidget::updateParam(const QVariant &/*value*/)
{
    //cl_log.log("updateParam", cl_logALWAYS);
}
