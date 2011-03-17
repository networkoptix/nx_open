#include "dlg_factory.h"

CLDeviceSettingsDlgFactory& CLDeviceSettingsDlgFactory::instance()
{
	static CLDeviceSettingsDlgFactory inst;
	return inst;
}

CLAbstractDeviceSettingsDlg* CLDeviceSettingsDlgFactory::createDlg(CLDevice* dev)
{
	for (int i = 0; i < mMaunufactures.count();++i)
	{
		CLAbstractDlgManufacture* man = mMaunufactures.at(i);
		if (man->canProduceDlg(dev))
			return man->createDlg(dev);
	}

	return 0;
}

void CLDeviceSettingsDlgFactory::registerDlgManufacture(CLAbstractDlgManufacture* manufacture)
{
	mMaunufactures.push_back(manufacture);
}

