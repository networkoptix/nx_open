#include "widgets.h"
#include "device\device.h"
#include <QHBoxLayout>
#include <QCheckBox>
#include <QGroupBox>
#include <QSlider>
#include <QRadioButton>
#include "base/log.h"
#include "settings.h"



SettingsSlider::SettingsSlider( Qt::Orientation orientation, QWidget * parent):
QSlider(orientation, parent)
{

}

void SettingsSlider::keyReleaseEvent ( QKeyEvent * event )
{
	emit onKeyReleased();
}
//==============================================

CLAbstractSettingsWidget::CLAbstractSettingsWidget(QObject* handler, CLDevice*dev, QString paramname):
mDevice(dev),
mHandler(handler)
{
	mParam = mDevice->getDevicePramList().get(paramname);

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

//==============================================
SettingsOnOffWidget::SettingsOnOffWidget(QObject* handler, CLDevice*dev, QString paramname):
CLAbstractSettingsWidget(handler, dev, paramname)
{
	QCheckBox * checkBox = new QCheckBox(mParam.name);
	if (mParam.value.default_value==mParam.value.possible_values.front())
		checkBox->setCheckState(Qt::Checked);

	QObject::connect(checkBox, SIGNAL(stateChanged ( int )), this, SLOT(stateChanged(int)));

	//QPalette plt;	plt.setColor(QPalette::WindowText, Qt::white);	checkBox->setPalette(plt);
	

	mWidget = checkBox;
	

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
		val = mParam.value.possible_values.front();
	}
	else
	{
		val = mParam.value.possible_values.back();
	}

	emit setParam(mParam.name,val);

}

//==============================================
SettingsMinMaxStepWidget::SettingsMinMaxStepWidget(QObject* handler, CLDevice*dev, QString paramname):
CLAbstractSettingsWidget(handler, dev, paramname)
{
	groupBox = new QGroupBox();


	slider = new SettingsSlider(Qt::Horizontal, groupBox);
	slider->setMinimumWidth(130);
	slider->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

	groupBox->setTitle(mParam.name);

	//QFont font("Courier New", 11);
	//groupBox->setFont(font);

	slider->setRange(mParam.value.min_val, mParam.value.max_val);
	slider->setValue(mParam.value.default_value);

	QString str;
	QTextStream(&str) << mParam.name<< "(" << (QString)mParam.value.default_value<< ")";
	groupBox->setTitle(str);


	QVBoxLayout *layout = new QVBoxLayout(groupBox);
	layout->addWidget(slider);



	QObject::connect(slider, SIGNAL(sliderReleased()), this, SLOT(onValChanged()));
	QObject::connect(slider, SIGNAL(onKeyReleased()), this, SLOT(onValChanged()));
	QObject::connect(slider, SIGNAL(valueChanged(int)), this, SLOT(onValChanged(int)) );

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
	emit setParam(mParam.name,slider->value());
}

//==============================================
SettingsEnumerationWidget::SettingsEnumerationWidget(QObject* handler, CLDevice*dev, QString paramname):
CLAbstractSettingsWidget(handler, dev, paramname)
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
		if (val == mParam.value.default_value)
			btn->setChecked(true);

		btn->setObjectName(val);
		btn->setFont(settings_font);

		connect(btn , SIGNAL(clicked()), this, SLOT(onClicked()));
	}
	
}

void SettingsEnumerationWidget::onClicked()
{
	QString val = QObject::sender()->objectName();

	emit setParam(mParam.name, val);
}