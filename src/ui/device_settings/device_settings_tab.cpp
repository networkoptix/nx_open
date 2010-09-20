#include "device_settings_tab.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QGroupBox>
#include "../../device/device.h"
#include "base/log.h"
#include "widgets.h"
#include "device_settings_dlg.h"
#include "settings.h"


CLDeviceSettingsTab::CLDeviceSettingsTab(CLAbstractDeviceSettingsDlg* dlg, QObject* handler, CLDevice* dev, QString group):
mDevice(dev),
mGroup(group),
mHandler(handler),
mDlg(dlg)
{
	setAutoFillBackground(true);
	QPalette palette;
	palette.setColor(backgroundRole(), Qt::black);
	//setPalette(palette);

	//QVBoxLayout *mainLayout = new QVBoxLayout;

	QList<QString> sub_groups = mDevice->getDevicePramList().subGroupList(mGroup);

	int size = 175;

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
			subgroupBox->setFont(settings_font);

			parent = subgroupBox;

			x+=(size+15);
		}

		CLParamList::MAP paramLst = mDevice->getDevicePramList().paramList(group, sub_group).list();

		int y = 0;
		foreach (CLParam param, paramLst)
		{
			if (!param.value.ui)	
				continue;

			CLAbstractSettingsWidget* awidget = 0;
			

			switch(param.value.type)
			{
			case CLParamType::OnOff:
				awidget =  new SettingsOnOffWidget(handler, dev, param.name);
				break;

			case CLParamType::MinMaxStep:
				awidget =  new SettingsMinMaxStepWidget(handler, dev, param.name);
				break;

			case CLParamType::Enumeration:
				awidget =  new SettingsEnumerationWidget(handler, dev, param.name);
				break;

			}


			if (awidget==0)
				continue;

			QWidget* widget = awidget->toWidget();
			widget->setFont(settings_font);

			if (param.value.description!="")
				widget->setToolTip(param.value.description);

			widget->setParent(parent);
			widget->move(10, 20 + y);
			y+=80;

			mDlg->putWidget(awidget);

		}



		//mainLayout->addWidget(subgroupBox);

		

	}

	//setLayout(mainLayout);
}

CLDeviceSettingsTab::~CLDeviceSettingsTab()
{

}

QString CLDeviceSettingsTab::name() const
{
	return mGroup;
}