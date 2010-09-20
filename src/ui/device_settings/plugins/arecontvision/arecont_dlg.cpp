#include "arecont_dlg.h"
#include "device\device.h"


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

CLAbstractDeviceSettingsDlg* AreconVisionDlgManufacture::getDlg(CLDevice* dev)
{
	return new AVSettingsDlg(dev);
}

bool AreconVisionDlgManufacture::canProduce(CLDevice* dev) const
{
	return mPossibleNames.contains(dev->getFullName());
}

//=======================================================================================
AVSettingsDlg::AVSettingsDlg(CLDevice* dev):
CLAbstractDeviceSettingsDlg(dev)
{

}