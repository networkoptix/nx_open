#include "widgets.h"

#include <QtGui/QBoxLayout>
#include <QtGui/QCheckBox>
#include <QtGui/QGroupBox>
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

CLAbstractSettingsWidget::CLAbstractSettingsWidget(QObject* handler, QnResourcePtr dev, QString group, QString sub_group, QString paramname)
    : QObject(),
mDevice(dev),
mHandler(handler),
m_group(group),
m_sub_group(sub_group),
mParam(mDevice->getResourceParamList().get(paramname))
{
    QObject::connect(this, SIGNAL( setParam(const QString&, const QnValue&) ),
                    handler, SLOT( setParam(const QString&, const QnValue&) )  );
}

CLAbstractSettingsWidget::~CLAbstractSettingsWidget()
{
}

QnResourcePtr CLAbstractSettingsWidget::getDevice() const
{
    return mDevice;
}

const QnParam& CLAbstractSettingsWidget::param() const
{
    return mParam;
}

QWidget* CLAbstractSettingsWidget::toWidget()
{
    return mWidget;
}

void CLAbstractSettingsWidget::setParam_helper(const QString& /*name*/, const QnValue& val)
{
    mParam.setValue(val);
    emit setParam(mParam.name(),val);
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
SettingsOnOffWidget::SettingsOnOffWidget(QObject* handler, QnResourcePtr dev, QString group, QString sub_group, QString paramname):
CLAbstractSettingsWidget(handler, dev, group, sub_group, paramname)
{
    m_checkBox = new QCheckBox(mParam.name());
    if (mParam.value() == mParam.possibleValues().front())
        m_checkBox->setCheckState(Qt::Checked);

    QObject::connect(m_checkBox, SIGNAL(stateChanged ( int )), this, SLOT(stateChanged(int)));

    //QPalette plt;	plt.setColor(QPalette::WindowText, Qt::white);	checkBox->setPalette(plt);//black

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
    setParam_helper(mParam.name(),val);
}

void SettingsOnOffWidget::updateParam(QString val)
{
    m_checkBox->setChecked(val == mParam.possibleValues().front()); // emits stateChanged
}

//==============================================
SettingsMinMaxStepWidget::SettingsMinMaxStepWidget(QObject* handler, QnResourcePtr dev, QString group, QString sub_group, QString paramname):
CLAbstractSettingsWidget(handler, dev, group, sub_group, paramname)
{
    groupBox = new QGroupBox();

    m_slider = new SettingsSlider(Qt::Horizontal, groupBox);
    m_slider->setMinimumWidth(130);
    m_slider->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    groupBox->setTitle(mParam.name());

    //QFont font("Courier New", 11);
    //groupBox->setFont(font);

    m_slider->setRange(mParam.minValue(), mParam.maxValue());
    m_slider->setValue(mParam.value().toInt());

    QVBoxLayout *layout = new QVBoxLayout(groupBox);
    layout->addWidget(m_slider);

    QObject::connect(m_slider, SIGNAL(sliderReleased()), this, SLOT(onValChanged()));
    QObject::connect(m_slider, SIGNAL(onKeyReleased()), this, SLOT(onValChanged()));
    QObject::connect(m_slider, SIGNAL(valueChanged(int)), this, SLOT(onValChanged(int)) );

    mWidget = groupBox;

    onValChanged(mParam.value().toInt());
}

void SettingsMinMaxStepWidget::onValChanged(int val)
{
    groupBox->setTitle(mParam.name() + QLatin1Char('(') + QString::number(val) + QLatin1Char(')'));
}

void SettingsMinMaxStepWidget::onValChanged()
{
    setParam_helper(mParam.name(),m_slider->value());
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
    mWidget = groupBox;
    groupBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    groupBox->setTitle(mParam.name());
    groupBox->setMinimumWidth(140);

    QVBoxLayout *layout = new QVBoxLayout(groupBox);

    for (int i = 0; i < mParam.uiPossibleValues().count();++i)
    {
        QRadioButton *btn = new QRadioButton(mParam.uiPossibleValues().at(i).toString(), groupBox);
        layout->addWidget(btn);

        QString val = mParam.possibleValues().at(i).toString();
        if (val == mParam.value().toString())
            btn->setChecked(true);

        btn->setObjectName(val);
        btn->setFont(settings_font);

        m_radioBtns.push_back(btn);

        connect(btn , SIGNAL(clicked()), this, SLOT(onClicked()));
    }
}

void SettingsEnumerationWidget::onClicked()
{
    QString val = QObject::sender()->objectName();

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
        if (btn->objectName()==name)
            return btn;
    }

    return 0;
}

//==================================================
SettingsButtonWidget::SettingsButtonWidget(QObject* handler, QnResourcePtr dev, QString group, QString sub_group, QString paramname):
CLAbstractSettingsWidget(handler, dev, group, sub_group, paramname)
{
    QPushButton* btn = new QPushButton(mParam.name());

    QObject::connect(btn, SIGNAL(released()), this, SLOT(onClicked()));

    btn->setFocusPolicy(Qt::NoFocus);

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
