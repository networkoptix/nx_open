#include "device_settings_tab.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QGroupBox>
#include "../../device/device.h"
#include "base/log.h"
#include "widgets.h"


CLDeviceSettingsTab::CLDeviceSettingsTab(QObject* handler, CLDevice* dev, QString group):
mDevice(dev),
mGroup(group),
mHandler(handler)
{
	setAutoFillBackground(true);
	QPalette palette;
	palette.setColor(backgroundRole(), Qt::black);
	//setPalette(palette);

	//QVBoxLayout *mainLayout = new QVBoxLayout;

	QList<QString> sub_groups = mDevice->getDevicePramList().subGroupList(mGroup);

	int size = 155;

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
			subgroupBox->setFixedHeight(420);
			subgroupBox->setTitle(sub_group);
			subgroupBox->setObjectName(group+sub_group);
			subgroupBox->move(10+x, 10);

			parent = subgroupBox;

			x+=170;
		}

		CLParamList::MAP paramLst = mDevice->getDevicePramList().paramList(group, sub_group).list();

		int y = 0;
		foreach (CLParam param, paramLst)
		{
			if (!param.value.ui)	
				continue;

			QWidget* widget = 0;


			switch(param.value.type)
			{
			case CLParamType::OnOff:
				widget =  (new SettingsOnOffWidget(handler, dev, param.name))->toWidget();
				break;

			case CLParamType::MinMaxStep:
				widget =  (new SettingsMinMaxStepWidget(handler, dev, param.name))->toWidget();
				break;
			}


			if (widget==0)
				continue;

			if (param.value.description!="")
				widget->setToolTip(param.value.description);

			widget->setParent(parent);
			widget->move(10, 20 + y);
			y+=50;


		}



		//mainLayout->addWidget(subgroupBox);

		

	}

	//setLayout(mainLayout);
}

CLDeviceSettingsTab::~CLDeviceSettingsTab()
{

}
