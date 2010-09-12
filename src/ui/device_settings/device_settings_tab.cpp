#include "device_settings_tab.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QGroupBox>
#include "../../device/device.h"
#include "../base/rand.h"
#include "settigs_widget_factory.h"

CLDeviceSettingsTab::CLDeviceSettingsTab(CLDevice* dev, QString group):
mDevice(dev),
mGroup(group)
{
	setAutoFillBackground(true);
	QPalette palette;
	palette.setColor(backgroundRole(), Qt::black);
	//setPalette(palette);

	//QVBoxLayout *mainLayout = new QVBoxLayout;

	QList<QString> sub_groups = mDevice->getDevicePramList().subGroupList(mGroup);

	int size = 200;

	QRect parent_rect = rect();

	int x = 0;
	for (int i = 0 ; i < sub_groups.count(); ++i)
	{
		QString sub_group = sub_groups.at(i);

		QWidget* parent = this;
		

		if (sub_group!="")
		{
			QGroupBox *subgroupBox = new QGroupBox(this);
			subgroupBox->setFixedWidth(size);
			subgroupBox->setFixedHeight(300);
			subgroupBox->setTitle(sub_group);
			subgroupBox->setObjectName(group+sub_group);
			subgroupBox->move(10+x, 10);

			parent = subgroupBox;

			x+=210;
		}

		CLParamList::MAP paramLst = mDevice->getDevicePramList().paramList(group, sub_group).list();

		int y = 0;
		foreach (CLParam param, paramLst)
		{
			if (param.value.http=="")
				continue;

			QWidget* widget = CLSettingsWidgetFactory::getWidget(param, this);
			if (widget==0)
				continue;

			if (param.value.description!="")
				widget->setToolTip(param.value.description);

			widget->setParent(parent);
			widget->move(10, 20 + y);
			y+=50;

			
			widget->setObjectName(param.name);


		}



		//mainLayout->addWidget(subgroupBox);

		

	}

	//setLayout(mainLayout);
}

CLDeviceSettingsTab::~CLDeviceSettingsTab()
{

}



void CLDeviceSettingsTab::onOnOffStateChanged(int state)
{
	QObject* obg = QObject::sender();
	QString param_name = obg->objectName();
	const CLParam param = mDevice->getDevicePramList().get(param_name);

	QString val;

	if (state == Qt::Checked)
	{
		val = param.value.possible_values.front();
	}
	else
	{
		val = param.value.possible_values.back();
	}

	mDevice->setParam_asynch(param_name,val);
}