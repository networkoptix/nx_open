#include "widgets.h"
#include "device/device.h"
#include "base/log.h"
#include "settings.h"

SettingsSlider::SettingsSlider( Qt::Orientation orientation, QWidget * parent):
QSlider(orientation, parent)
{

}

void SettingsSlider::keyReleaseEvent ( QKeyEvent * /*event*/ )
{
	emit onKeyReleased();
}
//==============================================

CLAbstractSettingsWidget::CLAbstractSettingsWidget(QObject* handler, CLDevice*dev, QString group, QString sub_group, QString paramname):
mDevice(dev),
mHandler(handler),
m_group(group),
m_sub_group(sub_group)
{
	mParam = mDevice->getDeviceParamList().get(paramname);

	QObject::connect(this, SIGNAL( setParam(const QString&, const CLValue&) ), 
					handler, SLOT( setParam(const QString&, const CLValue&) )  );
}

CLAbstractSettingsWidget::~CLAbstractSettingsWidget()
{

}

CLDevice* CLAbstractSettingsWidget::getDevice() const
{
	return mDevice;
}

const CLParam& CLAbstractSettingsWidget::param() const
{
	return mParam;
}

QWidget* CLAbstractSettingsWidget::toWidget()
{
	return mWidget;
}

void CLAbstractSettingsWidget::setParam_helper(const QString& name, const CLValue& val)
{
	mParam.value.value = val;
	emit setParam(mParam.name,val);
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
SettingsOnOffWidget::SettingsOnOffWidget(QObject* handler, CLDevice*dev, QString group, QString sub_group, QString paramname):
CLAbstractSettingsWidget(handler, dev, group, sub_group, paramname)
{
	m_checkBox = new QCheckBox(mParam.name);
	if (mParam.value.value==mParam.value.possible_values.front())
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
	if (mParam.value.possible_values.count()<2)
	{
		cl_log.log("param.value.possible_values.count()<2 !!!!", cl_logERROR);
		return;
	}

	QString val;

	if (state == Qt::Checked)
	{
		val = (QString)mParam.value.possible_values.front();
	}
	else
	{
		val = (QString)mParam.value.possible_values.back();
	}

	setParam_helper(mParam.name,val);

}

void SettingsOnOffWidget::updateParam(QString val)
{
    if (val==mParam.value.possible_values.front())
        m_checkBox->setChecked(true); // it emits stateChanged
    else
        m_checkBox->setChecked(false); // it emits stateChanged
}

//==============================================
SettingsMinMaxStepWidget::SettingsMinMaxStepWidget(QObject* handler, CLDevice*dev, QString group, QString sub_group, QString paramname):
CLAbstractSettingsWidget(handler, dev, group, sub_group, paramname)
{
	groupBox = new QGroupBox();

	m_slider = new SettingsSlider(Qt::Horizontal, groupBox);
	m_slider->setMinimumWidth(130);
	m_slider->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

	groupBox->setTitle(mParam.name);

	//QFont font("Courier New", 11);
	//groupBox->setFont(font);

	m_slider->setRange(mParam.value.min_val, mParam.value.max_val);
	m_slider->setValue(mParam.value.value);

	QString str;
	QTextStream(&str) << mParam.name<< "(" << (QString)mParam.value.value<< ")";
	groupBox->setTitle(str);

	QVBoxLayout *layout = new QVBoxLayout(groupBox);
	layout->addWidget(m_slider);

	QObject::connect(m_slider, SIGNAL(sliderReleased()), this, SLOT(onValChanged()));
	QObject::connect(m_slider, SIGNAL(onKeyReleased()), this, SLOT(onValChanged()));
	QObject::connect(m_slider, SIGNAL(valueChanged(int)), this, SLOT(onValChanged(int)) );

	mWidget = groupBox;
}

void SettingsMinMaxStepWidget::onValChanged(int val)
{
	QString str;
	QTextStream(&str) << mParam.name<< "(" << val << ")";
	groupBox->setTitle(str);
}

void SettingsMinMaxStepWidget::onValChanged()
{
	setParam_helper(mParam.name,m_slider->value());
}

void SettingsMinMaxStepWidget::updateParam(QString val)
{
    CLValue cv(val);
    m_slider->setValue(cv);
}

//==============================================
SettingsEnumerationWidget::SettingsEnumerationWidget(QObject* handler, CLDevice*dev, QString group, QString sub_group, QString paramname):
CLAbstractSettingsWidget(handler, dev, group, sub_group, paramname)
{
	QGroupBox* groupBox  = new QGroupBox();
	mWidget = groupBox;
	groupBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	groupBox->setTitle(mParam.name);
	groupBox->setMinimumWidth(140);

	QVBoxLayout *layout = new QVBoxLayout(groupBox);

	for (int i = 0; i < mParam.value.ui_possible_values.count();++i)
	{
		QRadioButton *btn = new QRadioButton(mParam.value.ui_possible_values.at(i), groupBox);
		layout->addWidget(btn);

		QString val = mParam.value.possible_values.at(i);
		if (val == mParam.value.value)
			btn->setChecked(true);

		btn->setObjectName(val);
		btn->setFont(settings_font);

		m_radioBtns.push_back(btn);

        connect(btn , SIGNAL(clicked()), this, SLOT(onClicked()));
     
	}

	//QPushButton* btnt = new QPushButton("test");
	//btnt->select
	//layout->addWidget(btnt);

}

void SettingsEnumerationWidget::onClicked()
{
	QString val = QObject::sender()->objectName();

	setParam_helper(mParam.name, val);
}

void SettingsEnumerationWidget::updateParam(QString val)
{
    QRadioButton* btn = getBtnByname(val);
    if (btn)
        btn->setChecked(true);
}

QRadioButton* SettingsEnumerationWidget::getBtnByname(const QString& name)
{
    foreach(QRadioButton* btn, m_radioBtns)
    {
        if (btn->objectName()==name)
            return btn;
    }

    return 0;
}

//==================================================
SettingsButtonWidget::SettingsButtonWidget(QObject* handler, CLDevice*dev, QString group, QString sub_group, QString paramname):
CLAbstractSettingsWidget(handler, dev, group, sub_group, paramname)
{
	QPushButton* btn = new QPushButton(mParam.name);

	QObject::connect(btn, SIGNAL(released()), this, SLOT(onClicked()));

	btn->setFocusPolicy(Qt::NoFocus);

	mWidget = btn;

}

void SettingsButtonWidget::onClicked()
{
	setParam_helper(mParam.name, "");
}

void SettingsButtonWidget::updateParam(QString /*val*/)
{
    //cl_log.log("updateParam", cl_logALWAYS);
}
