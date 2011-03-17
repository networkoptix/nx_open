#include "arecont_dlg.h"
#include "device\device.h"
#include "..\..\widgets.h"
#include "settings.h"

AreconVisionDlgManufacture::AreconVisionDlgManufacture()
{
	mPossibleNames.push_back("1300");
	mPossibleNames.push_back("2100");
	mPossibleNames.push_back("3100");
	mPossibleNames.push_back("5100");

	mPossibleNames.push_back("1310");
	mPossibleNames.push_back("2110");
	mPossibleNames.push_back("3110");
	mPossibleNames.push_back("5110");

	mPossibleNames.push_back("1305");
	mPossibleNames.push_back("2105");
	mPossibleNames.push_back("3105");
	mPossibleNames.push_back("5105");
	mPossibleNames.push_back("10005");

	mPossibleNames.push_back("1355");
	mPossibleNames.push_back("2155");
	mPossibleNames.push_back("3155");
	mPossibleNames.push_back("5155");

	mPossibleNames.push_back("1315");
	mPossibleNames.push_back("2115");
	mPossibleNames.push_back("3115");
	mPossibleNames.push_back("5115");

	mPossibleNames.push_back("1325");
	mPossibleNames.push_back("2125");
	mPossibleNames.push_back("3125");
	mPossibleNames.push_back("5125");

	mPossibleNames.push_back("2805");
	mPossibleNames.push_back("2815");
	mPossibleNames.push_back("2825");

	mPossibleNames.push_back("8360");
	mPossibleNames.push_back("8180");

	mPossibleNames.push_back("8365");
	mPossibleNames.push_back("8185");

	mPossibleNames.push_back("3130");
	mPossibleNames.push_back("3135");

	QList<QString> base = mPossibleNames;

	foreach(QString str, base)
	{
		QString mini = str + "M";
		QString dn = str + "DN";
		QString ai = str + "AI";
		QString ir = str + "IR";

		mPossibleNames.push_back(mini);
		mPossibleNames.push_back(dn);
		mPossibleNames.push_back(ai);
		mPossibleNames.push_back(ir);
	}

}

AreconVisionDlgManufacture& AreconVisionDlgManufacture::instance()
{
	static AreconVisionDlgManufacture inst;
	return inst;
}

CLAbstractDeviceSettingsDlg* AreconVisionDlgManufacture::createDlg(CLDevice* dev)
{
	return new AVSettingsDlg(dev);
}

bool AreconVisionDlgManufacture::canProduceDlg(CLDevice* dev) const
{
	return mPossibleNames.contains(dev->getName());
}

//=======================================================================================
AVSettingsDlg::AVSettingsDlg(CLDevice* dev):
CLAbstractDeviceSettingsDlg(dev)
{
	initTabsOrder();
	initImageQuality();
	initExposure();
	initAI();
	initDN();
	initMD();
	initAdmin();

	correctWgtsState();
}

void AVSettingsDlg::initTabsOrder()
{
	//return;

	CLDeviceSettingsTab* tab = 0;

	if (tab=tabByName(tr("Image quality")))
		addTabWidget(tab);

	if (tab=tabByName(tr("Exposure")))
		addTabWidget(tab);

	if (tab=tabByName(tr("AutoIris")))
		addTabWidget(tab);

	if (tab=tabByName(tr("Day/Night")))
		addTabWidget(tab);

	if (tab=tabByName(tr("Binning")))
		addTabWidget(tab);

	if (tab=tabByName(tr("Motion detection")))
		addTabWidget(tab);

	if (tab=tabByName(tr("Network")))
		addTabWidget(tab);

	if (tab=tabByName(tr("Administration/Info")))
		addTabWidget(tab);

}

void AVSettingsDlg::initImageQuality()
{
	QGroupBox* group;
	CLAbstractSettingsWidget* wgt;

	if (group=getGroupByName("Quality"))
	{
		group->move(10,10);
		group->setFixedWidth(350);
		group->setFixedHeight(205);

		bool h264 = (getWidgetByName("Codec")!=0);

		if (wgt = getWidgetByName("Quality"))
		{
			if (h264)
				wgt->toWidget()->move(10,35);
			else
			{
				wgt->toWidget()->move(10,73);
				wgt->toWidget()->setFixedWidth(330);
			}

		}

		if (wgt = getWidgetByName("Codec"))
			wgt->toWidget()->move(200,25);

		if (wgt = getWidgetByName("Bitrate"))
		{
			wgt->toWidget()->move(10,125);
			wgt->toWidget()->setFixedWidth(330);
		}

	}

	if (group=getGroupByName("Color"))
	{
		group->move(385,10);
		group->setFixedWidth(180);
		group->setFixedHeight(205);

		if (wgt = getWidgetByName("Red"))
			wgt->toWidget()->move(10,30);

		if (wgt = getWidgetByName("Blue"))
			wgt->toWidget()->move(10,120);

	}

	if (group=getGroupByName("Adjustment"))
	{
		group->move(10,240);
		group->setFixedWidth(555);
		group->setFixedHeight(110);

		if (wgt = getWidgetByName("Brightness"))
			wgt->toWidget()->move(10,30);

		if (wgt = getWidgetByName("Sharpness"))
			wgt->toWidget()->move(200,30);

		if (wgt = getWidgetByName("Saturation"))
			wgt->toWidget()->move(380,30);

	}

	if (wgt = getWidgetByName("Equalize Brightness"))
	{
		wgt->toWidget()->move(135,360);

		if (wgt = getWidgetByName("Equalize Color"))
			wgt->toWidget()->move(340,360);

		if (wgt = getWidgetByName("Rotate 180"))
			wgt->toWidget()->hide(); // for now

	}
	else
	{
		if (wgt = getWidgetByName("Rotate 180"))
			wgt->toWidget()->move(255,360);
	}

	//

}

void AVSettingsDlg::initExposure()
{

	QGroupBox* group;
	CLAbstractSettingsWidget* wgt;

	if (wgt = getWidgetByName("Illumination"))
		wgt->toWidget()->move(420,60);

	if (wgt = getWidgetByName("Lighting"))
		wgt->toWidget()->move(420,210);

	if (group=getGroupByName("Low Light Mode"))
	{
		group->move(10,60);
		group->setFixedWidth(380);
		group->setFixedHeight(240);

		if (wgt = getWidgetByName("Light Mode"))
			wgt->toWidget()->move(30,30);

		if (wgt = getWidgetByName("Short Exposure"))
			wgt->toWidget()->move(200,75);

		if (wgt = getWidgetByName("Auto exposure On/Off"))
			wgt->toWidget()->move(30,200);

	}

}

void AVSettingsDlg::initAI()
{
	QGroupBox* group;
	CLAbstractSettingsWidget* wgt;

	if (wgt = getWidgetByName("AutoIris enable"))
		wgt->toWidget()->move(200,180);

}

void AVSettingsDlg::initDN()
{
	QGroupBox* group;
	CLAbstractSettingsWidget* wgt;

	if (wgt = getWidgetByName("Day/Night Mode"))
		wgt->toWidget()->move(215,130);
}

void AVSettingsDlg::initMD()
{
	QGroupBox* group;
	CLAbstractSettingsWidget* wgt;

	//Enable motion detection
	//Zone size
	//Threshold
	//Sensitivity
	//Md detail

	if (wgt = getWidgetByName("Enable motion detection"))
		wgt->toWidget()->move(175,30);

	if (wgt = getWidgetByName("Zone size"))
		wgt->toWidget()->move(80,80);

	if (wgt = getWidgetByName("Threshold"))
		wgt->toWidget()->move(320,80);

	if (wgt = getWidgetByName("Sensitivity"))
		wgt->toWidget()->move(80,210);

	if (wgt = getWidgetByName("Md detail"))
		wgt->toWidget()->move(320,210);

}

void AVSettingsDlg::initAdmin()
{
	QGroupBox* group;
	CLAbstractSettingsWidget* wgt;

	if (wgt = getWidgetByName("Factory defaults"))
		wgt->toWidget()->move(230,150);

}

//=========================================================================
void AVSettingsDlg::setParam(const QString& name, const CLValue& val)
{
	CLAbstractDeviceSettingsDlg::setParam(name, val);
	correctWgtsState();
}

void AVSettingsDlg::onSuggestions()
{
	QString text = tr("To reduce the bandwidth try to set Light Mode on Exposure tab to HightSpeed and set Short Exposure to 30ms.");
	QMessageBox mbox ( QMessageBox::Information, tr("Suggestion"), text, QMessageBox::Ok, this, Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint );
	mbox.setWindowOpacity(global_dlg_opacity);

	mbox.exec();

}

void AVSettingsDlg::onClose()
{
    CLAbstractDeviceSettingsDlg::setParam("Save to flash", "");
    CLAbstractDeviceSettingsDlg::close();
}

void AVSettingsDlg::correctWgtsState()
{
	CLAbstractSettingsWidget* wgt;
	QString val;

	if (wgt = getWidgetByName("Light Mode"))
	{
		val  = wgt->param().value.value;

		if (val==QString("highspeed"))
		{
			if (wgt = getWidgetByName("Short Exposure"))
				wgt->toWidget()->show();
		}
		else
		{
			if (wgt = getWidgetByName("Short Exposure"))
				wgt->toWidget()->hide();
		}
	}

	//=================================================
	if (wgt = getWidgetByName("Codec"))
	{
		val  = wgt->param().value.value;

		if (val==QString("H.264"))
		{
			if (wgt = getWidgetByName("Bitrate"))
				wgt->toWidget()->show();
		}
		else
		{
			if (wgt = getWidgetByName("Bitrate"))
				wgt->toWidget()->hide();
		}
	}

}