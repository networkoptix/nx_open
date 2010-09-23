#include "arecont_dlg.h"
#include "device\device.h"
#include <QGroupBox>
#include "..\..\widgets.h"


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

	mPossibleNames.push_back("2805");
	mPossibleNames.push_back("2815");

	mPossibleNames.push_back("8360");
	mPossibleNames.push_back("8180");

	mPossibleNames.push_back("8365");
	mPossibleNames.push_back("8185");

	mPossibleNames.push_back("3130");
	mPossibleNames.push_back("3135");


	//=======dn===========================
	mPossibleNames.push_back("1300DN");
	mPossibleNames.push_back("2100DN");
	mPossibleNames.push_back("3100DN");
	mPossibleNames.push_back("5100DN");

	mPossibleNames.push_back("1310DN");
	mPossibleNames.push_back("2110DN");
	mPossibleNames.push_back("3110DN");
	mPossibleNames.push_back("5110DN");


	mPossibleNames.push_back("1305DN");
	mPossibleNames.push_back("2105DN");
	mPossibleNames.push_back("3105DN");
	mPossibleNames.push_back("5105DN");
	mPossibleNames.push_back("10005DN");

	mPossibleNames.push_back("1355DN");
	mPossibleNames.push_back("2155DN");
	mPossibleNames.push_back("3155DN");
	mPossibleNames.push_back("5155DN");

	mPossibleNames.push_back("1315DN");
	mPossibleNames.push_back("2115DN");
	mPossibleNames.push_back("3115DN");
	mPossibleNames.push_back("5115DN");

	mPossibleNames.push_back("2805DN");
	mPossibleNames.push_back("2815DN");


	//=======ai===========================
	mPossibleNames.push_back("1300AI");
	mPossibleNames.push_back("2100AI");
	mPossibleNames.push_back("3100AI");
	mPossibleNames.push_back("5100AI");

	mPossibleNames.push_back("1310AI");
	mPossibleNames.push_back("2110AI");
	mPossibleNames.push_back("3110AI");
	mPossibleNames.push_back("5110AI");


	mPossibleNames.push_back("1305AI");
	mPossibleNames.push_back("2105AI");
	mPossibleNames.push_back("3105AI");
	mPossibleNames.push_back("5105AI");
	mPossibleNames.push_back("10005AI");

	mPossibleNames.push_back("1355AI");
	mPossibleNames.push_back("2155AI");
	mPossibleNames.push_back("3155AI");
	mPossibleNames.push_back("5155AI");

	mPossibleNames.push_back("1315AI");
	mPossibleNames.push_back("2115AI");
	mPossibleNames.push_back("3115AI");
	mPossibleNames.push_back("5115AI");

	mPossibleNames.push_back("2805AI");
	mPossibleNames.push_back("2815AI");







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
	return mPossibleNames.contains(dev->getFullName());
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

}

void AVSettingsDlg::initTabsOrder()
{
	//return;

	CLDeviceSettingsTab* tab = 0;

	if (tab=tabByName(tr("Image quality")))
		addTab(tab);

	if (tab=tabByName(tr("Exposure")))
		addTab(tab);

	if (tab=tabByName(tr("AutoIris")))
		addTab(tab);

	if (tab=tabByName(tr("Day/Night")))
		addTab(tab);

	if (tab=tabByName(tr("Binning")))
		addTab(tab);

	if (tab=tabByName(tr("Motion detection")))
		addTab(tab);


	if (tab=tabByName(tr("Network")))
		addTab(tab);

	if (tab=tabByName(tr("Administration")))
		addTab(tab);

	if (tab=tabByName(tr("Camera Info")))
		addTab(tab);

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


	if (wgt = getWidgetByName("Rotate 180"))
		wgt->toWidget()->move(255,360);

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
		wgt->toWidget()->move(200,200);

	
}

void AVSettingsDlg::initDN()
{
	QGroupBox* group;
	CLAbstractSettingsWidget* wgt;

	if (wgt = getWidgetByName("Day/Night Mode"))
		wgt->toWidget()->move(200,200);


}
