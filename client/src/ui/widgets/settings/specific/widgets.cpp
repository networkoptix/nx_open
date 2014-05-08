#include "widgets.h"

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>

#include "client/client_settings.h"
#include "ui/style/globals.h"

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

CLAbstractSettingsWidget::CLAbstractSettingsWidget(const QnParam &param, QWidget *parent)
    : QWidget(parent),
      m_param(param)
{
}

CLAbstractSettingsWidget::~CLAbstractSettingsWidget()
{
}

const QnParam &CLAbstractSettingsWidget::param() const
{
    return m_param;
}

QVariant CLAbstractSettingsWidget::paramValue() const
{
    return m_param.value();
}

void CLAbstractSettingsWidget::setParamValue(const QVariant &value)
{
    m_param.setValue(value);
    Q_EMIT paramValueChanged(value);
    updateParam(value);
}

//==============================================

SettingsOnOffWidget::SettingsOnOffWidget(const QnParam &param, QWidget *parent)
    : CLAbstractSettingsWidget(param, parent)
{
    m_checkBox = new QCheckBox(param.name());

    updateParam(param.value());

    connect(m_checkBox, SIGNAL(toggled(bool)), this, SLOT(toggled(bool)));

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(m_checkBox);
    setLayout(layout);
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
SettingsMinMaxStepWidget::SettingsMinMaxStepWidget(const QnParam &param, QWidget *parent)
    : CLAbstractSettingsWidget(param, parent),
      m_slider(0),
      m_groupBox(0)
{
    m_groupBox = new QGroupBox(param.name());
    //m_groupBox->setFont(QFont("Courier New", 11));

    m_slider = new SettingsSlider(Qt::Horizontal, m_groupBox);

    connect(m_slider, SIGNAL(sliderReleased()), this, SLOT(onValueChanged()));
    connect(m_slider, SIGNAL(onKeyReleased()), this, SLOT(onValueChanged()));
    connect(m_slider, SIGNAL(valueChanged(int)), this, SLOT(onValueChanged(int)));

    m_slider->setMinimumWidth(130);
    m_slider->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_slider->setRange(param.minValue(), param.maxValue());
    m_slider->setValue(param.value().toInt());


    QVBoxLayout *groupBoxLayout = new QVBoxLayout;
    groupBoxLayout->addWidget(m_slider);
    m_groupBox->setLayout(groupBoxLayout);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(m_groupBox);
    setLayout(layout);
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
SettingsEnumerationWidget::SettingsEnumerationWidget(const QnParam &param, QWidget *parent)
    : CLAbstractSettingsWidget(param, parent)
{
    QGroupBox *groupBox = new QGroupBox(param.name());
    groupBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    groupBox->setMinimumWidth(140);

    QVBoxLayout *groupBoxLayout = new QVBoxLayout;
    foreach (const QVariant &val, param.uiPossibleValues()) {
        const QString name = val.toString();

        QRadioButton *button = new QRadioButton(name, groupBox);
        button->setObjectName(name);
        button->setChecked(name == param.value().toString());

        connect(button, SIGNAL(clicked()), this, SLOT(onClicked()));

        groupBoxLayout->addWidget(button);

        m_radioButtons.append(button);
    }
    groupBox->setLayout(groupBoxLayout);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(groupBox);
    setLayout(layout);
}

void SettingsEnumerationWidget::onClicked()
{
    setParamValue(sender()->objectName());
}

void SettingsEnumerationWidget::updateParam(const QVariant &value)
{
    const QString buttonName = value.toString();
    foreach (QRadioButton *button, m_radioButtons) {
        if (button->objectName() == buttonName) {
            button->setChecked(true);
            break;
        }
    }
}

//==================================================
SettingsButtonWidget::SettingsButtonWidget(const QnParam &param, QWidget *parent)
    : CLAbstractSettingsWidget(param, parent)
{
    QPushButton *button = new QPushButton(param.name());
    button->setFocusPolicy(Qt::NoFocus);

    connect(button, SIGNAL(released()), this, SLOT(onClicked()));

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(button);
    setLayout(layout);
}

void SettingsButtonWidget::onClicked()
{
    setParamValue(QString());
}

void SettingsButtonWidget::updateParam(const QVariant &/*value*/)
{
    //cl_log.log("updateParam", cl_logALWAYS);
}


//==================================================
SettingsEditorFactory::SettingsEditorFactory() : QItemEditorFactory()
{
    //registerEditor(QVariant::Type(qMetaTypeId<QRegion>()), new QStandardItemEditorCreator<QnCameraMotionMaskWidget>());
    //registerEditor(QVariant::Type(qMetaTypeId<QnScheduleTaskList>()), new QStandardItemEditorCreator<QnCameraScheduleWidget>());
}

QWidget *SettingsEditorFactory::createEditor(const QnParam &param, QWidget *parent) const
{
    switch (param.type()) {
    case Qn::PDT_OnOff:
        return new SettingsOnOffWidget(param);

    case Qn::PDT_MinMaxStep:
        return new SettingsMinMaxStepWidget(param);

    case Qn::PDT_Enumeration:
        return new SettingsEnumerationWidget(param);

    case Qn::PDT_Button:
        return new SettingsButtonWidget(param);

    case Qn::PDT_Boolen: // default true/false combobox
    case Qn::PDT_Value: // value-based
    default:
        break;
    }

    QVariant::Type type = QVariant::Type(param.defaultValue().userType());
    return QItemEditorFactory::createEditor(type, parent);
}

QByteArray SettingsEditorFactory::valuePropertyName(const QnParam &param) const
{
    switch (param.type()) {
    case Qn::PDT_OnOff:
    case Qn::PDT_MinMaxStep:
    case Qn::PDT_Enumeration:
    case Qn::PDT_Button:
        return "paramValue";

    case Qn::PDT_Boolen: // default true/false combobox
    case Qn::PDT_Value: // value-based
    default:
        break;
    }

    QVariant::Type type = QVariant::Type(param.defaultValue().userType());
    return QItemEditorFactory::valuePropertyName(type);
}
