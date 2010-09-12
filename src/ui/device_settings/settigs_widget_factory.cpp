#include "settigs_widget_factory.h"
#include <QHBoxLayout>
#include <QCheckBox>
#include <QGroupBox>
#include <QSlider>

QWidget* CLSettingsWidgetFactory::getWidget(CLParam& param, QObject* handler)
{
	switch(param.value.type)
	{
	case CLParamType::OnOff:
		return getOnOffWidget(param, handler);

	case CLParamType::MinMaxStep:
		return getMinMaxStepWidget(param, handler);
	
		

	default:
	    return 0;
	}
}

QLayout* CLSettingsWidgetFactory::getLayout(CLParam& param, QObject* handler)
{
	switch(param.value.type)
	{

	default:
		return 0;
	}
}



QWidget* CLSettingsWidgetFactory::getOnOffWidget(CLParam& param, QObject* handler)
{
	
	QCheckBox * checkBox = new QCheckBox(param.name);
	if (param.value.default_value==param.value.possible_values.front())
		checkBox->setCheckState(Qt::Checked);

	QObject::connect(checkBox, SIGNAL(stateChanged ( int )), handler, SLOT(onOnOffStateChanged(int)));
	return checkBox;

}

QWidget* CLSettingsWidgetFactory::getMinMaxStepWidget(CLParam& param, QObject* handler)
{
	QGroupBox* groupBox = new QGroupBox();

	
	QSlider *slider = new QSlider(Qt::Horizontal, groupBox);
	slider->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

	groupBox->setTitle(param.name);
	slider->setRange(param.value.min_val, param.value.max_val);
	slider->setValue(param.value.default_value);

	QVBoxLayout *layout = new QVBoxLayout(groupBox);
	layout->addWidget(slider);


	return groupBox;
}