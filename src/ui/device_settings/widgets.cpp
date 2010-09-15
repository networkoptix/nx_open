#include "widgets.h"
#include "device\device.h"
#include <QHBoxLayout>
#include <QCheckBox>
#include <QGroupBox>
#include <QSlider>
#include "base/log.h"



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
	QGroupBox* groupBox = new QGroupBox();


	slider = new SettingsSlider(Qt::Horizontal, groupBox);
	slider->setMinimumWidth(150);
	slider->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

	groupBox->setTitle(mParam.name);
	slider->setRange(mParam.value.min_val, mParam.value.max_val);
	slider->setValue(mParam.value.default_value);

	QVBoxLayout *layout = new QVBoxLayout(groupBox);
	layout->addWidget(slider);



	QObject::connect(slider, SIGNAL(sliderReleased()), this, SLOT(onValChanged()));
	QObject::connect(slider, SIGNAL(onKeyReleased()), this, SLOT(onValChanged()));

	mWidget = groupBox;

}

void SettingsMinMaxStepWidget::onValChanged()
{
	emit setParam(mParam.name,slider->value());
}

//==============================================
