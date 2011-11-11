#include "device_settings_tab.h"
#include "widgets.h"
#include "device_settings_dlg.h"
#include "settings.h"

CLDeviceSettingsTab::CLDeviceSettingsTab(CLAbstractDeviceSettingsDlg* dlg, QObject* handler, QnResourcePtr dev, QString group):
mDevice(dev),
mGroup(group),
mHandler(handler),
mDlg(dlg)
{
	setAutoFillBackground(true);

    QPalette pal = palette();
    pal.setColor(backgroundRole(), Qt::black);
    //setPalette(pal);

	//QVBoxLayout *mainLayout = new QVBoxLayout;

	QList<QString> sub_groups = mDevice->getResourceParamList().subGroupList(mGroup);

	int size = 175;

	QRect parent_rect = rect();

	int x = 0;
	for (int i = 0 ; i < sub_groups.count(); ++i)
	{
		QString sub_group = sub_groups.at(i);

		QWidget* parent = this;

		if (!sub_group.isEmpty())
		{
			QGroupBox *subgroupBox = new QGroupBox(this);
			subgroupBox->setFixedWidth(size);
			subgroupBox->setFixedHeight(420);
			subgroupBox->setTitle(sub_group);
			subgroupBox->setObjectName(group+sub_group);
			subgroupBox->move(10+x, 10);
			subgroupBox->setFont(settings_font);

			mDlg->putGroup(subgroupBox);

			parent = subgroupBox;

			x+=(size+15);
		}

		QnParamList::QnParamMap paramLst = mDevice->getResourceParamList().paramList(group, sub_group).list();

		int y = 0;
		foreach (QnParam param, paramLst)
		{
			if (!param.isUiParam())
				continue;

			CLAbstractSettingsWidget* awidget = 0;

			switch(param.type())
			{
			case QnParamType::OnOff:
				awidget =  new SettingsOnOffWidget(handler, dev, group, sub_group, param.name());
				break;

			case QnParamType::MinMaxStep:
				awidget =  new SettingsMinMaxStepWidget(handler, dev, group, sub_group, param.name());
				break;

			case QnParamType::Enumeration:
				awidget =  new SettingsEnumerationWidget(handler, dev, group, sub_group, param.name());
				break;

			case QnParamType::Button:
				awidget =  new SettingsButtonWidget(handler, dev, group, sub_group, param.name());
				break;

			}

			if (awidget==0)
				continue;

			QWidget* widget = awidget->toWidget();
			widget->setFont(settings_font);

			if (!param.description().isEmpty())
				widget->setToolTip(param.description());

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
