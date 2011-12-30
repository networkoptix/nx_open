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

CLAbstractSettingsWidget::CLAbstractSettingsWidget(QObject* handler, QnResourcePtr resource, QString group, QString sub_group, QString paramname)
    : QObject(),
      mDevice(resource),
      mHandler(handler),
      m_group(group),
      m_sub_group(sub_group),
      mParam(mDevice->getResourceParamList().value(paramname))
{
    connect(this, SIGNAL(setParam(QString,QVariant)), handler, SLOT(setParam(QString,QVariant)));
}

CLAbstractSettingsWidget::~CLAbstractSettingsWidget()
{
}

QnResourcePtr CLAbstractSettingsWidget::resource() const
{
    return mDevice;
}

const QnParam& CLAbstractSettingsWidget::param() const
{
    return mParam;
}

QWidget *CLAbstractSettingsWidget::widget() const
{
    Q_ASSERT(mWidget);
    return mWidget;
}

void CLAbstractSettingsWidget::setParam_helper(const QString &name, const QVariant &val)
{
    mParam.setValue(val);
    Q_EMIT setParam(name, val);
}

QString CLAbstractSettingsWidget::group() const
{
    return m_group;
}

QString CLAbstractSettingsWidget::subGroup() const
{
    return m_sub_group;
}

//==============================================

SettingsOnOffWidget::SettingsOnOffWidget(QObject* handler, QnResourcePtr dev, QString group, QString sub_group, QString paramname)
    : CLAbstractSettingsWidget(handler, dev, group, sub_group, paramname)
{
    m_checkBox = new QCheckBox(mParam.name());
    if (!mParam.possibleValues().isEmpty() && mParam.value() == mParam.possibleValues().front())
        m_checkBox->setCheckState(Qt::Checked);

    connect(m_checkBox, SIGNAL(stateChanged(int)), this, SLOT(stateChanged(int)));

    mWidget = m_checkBox;
}

SettingsOnOffWidget::~SettingsOnOffWidget()
{
}

void SettingsOnOffWidget::stateChanged(int state)
{
    if (mParam.possibleValues().count()<2)
    {
        cl_log.log(QLatin1String("param.possible_values.count()<2 !!!!"), cl_logERROR);
        return;
    }

    QString val = state == Qt::Checked ? mParam.possibleValues().front().toString() : mParam.possibleValues().back().toString();
    setParam_helper(mParam.name(), val);
}

void SettingsOnOffWidget::updateParam(QString val)
{
    m_checkBox->setChecked(val == mParam.possibleValues().front()); // emits stateChanged
}

//==============================================
SettingsMinMaxStepWidget::SettingsMinMaxStepWidget(QObject* handler, QnResourcePtr dev, QString group, QString sub_group, QString paramname)
    : CLAbstractSettingsWidget(handler, dev, group, sub_group, paramname)
{
    groupBox = new QGroupBox();
    groupBox->setTitle(mParam.name());
    //groupBox->setFont(QFont("Courier New", 11));

    m_slider = new SettingsSlider(Qt::Horizontal, groupBox);
    m_slider->setMinimumWidth(130);
    m_slider->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_slider->setRange(mParam.minValue(), mParam.maxValue());
    m_slider->setValue(mParam.value().toInt());

    connect(m_slider, SIGNAL(sliderReleased()), this, SLOT(onValueChanged()));
    connect(m_slider, SIGNAL(onKeyReleased()), this, SLOT(onValChanged()));
    connect(m_slider, SIGNAL(valueChanged(int)), this, SLOT(onValueChanged(int)));

    QVBoxLayout *layout = new QVBoxLayout(groupBox);
    layout->addWidget(m_slider);

    mWidget = groupBox;

    onValueChanged(mParam.value().toInt());
}

void SettingsMinMaxStepWidget::onValueChanged(int val)
{
    groupBox->setTitle(mParam.name() + QLatin1Char('(') + QString::number(val) + QLatin1Char(')'));
}

void SettingsMinMaxStepWidget::onValueChanged()
{
    setParam_helper(mParam.name(), m_slider->value());
}

void SettingsMinMaxStepWidget::updateParam(QString val)
{
    m_slider->setValue(val.toInt());
}

//==============================================
SettingsEnumerationWidget::SettingsEnumerationWidget(QObject* handler, QnResourcePtr dev, QString group, QString sub_group, QString paramname):
CLAbstractSettingsWidget(handler, dev, group, sub_group, paramname)
{
    QGroupBox* groupBox  = new QGroupBox();
    groupBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    groupBox->setTitle(mParam.name());
    groupBox->setMinimumWidth(140);

    QVBoxLayout *layout = new QVBoxLayout(groupBox);
    foreach (const QVariant &val, mParam.uiPossibleValues())
    {
        const QString name = val.toString();

        QRadioButton *btn = new QRadioButton(name, groupBox);
        btn->setObjectName(name);
        btn->setFont(settings_font);
        btn->setChecked(name == mParam.value().toString());

        connect(btn , SIGNAL(clicked()), this, SLOT(onClicked()));

        layout->addWidget(btn);

        m_radioBtns.append(btn);
    }

    mWidget = groupBox;
}

void SettingsEnumerationWidget::onClicked()
{
    QString val = sender()->objectName();

    setParam_helper(mParam.name(), val);
}

void SettingsEnumerationWidget::updateParam(QString val)
{
    if (QRadioButton* btn = getBtnByname(val))
        btn->setChecked(true);
}

QRadioButton* SettingsEnumerationWidget::getBtnByname(const QString& name)
{
    foreach (QRadioButton *btn, m_radioBtns)
    {
        if (btn->objectName() == name)
            return btn;
    }

    return 0;
}

//==================================================
SettingsButtonWidget::SettingsButtonWidget(QObject* handler, QnResourcePtr dev, QString group, QString sub_group, QString paramname)
    : CLAbstractSettingsWidget(handler, dev, group, sub_group, paramname)
{
    QPushButton* btn = new QPushButton(mParam.name());
    btn->setFocusPolicy(Qt::NoFocus);

    connect(btn, SIGNAL(released()), this, SLOT(onClicked()));

    mWidget = btn;
}

void SettingsButtonWidget::onClicked()
{
    setParam_helper(mParam.name(), "");
}

void SettingsButtonWidget::updateParam(QString /*val*/)
{
    //cl_log.log("updateParam", cl_logALWAYS);
}
