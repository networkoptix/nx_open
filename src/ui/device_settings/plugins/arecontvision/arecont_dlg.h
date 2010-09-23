#ifndef arecon_dlg_factory_h1733
#define arecon_dlg_factory_h1733

#include "../../dlg_factory.h"
#include "../../device_settings_dlg.h"

class AreconVisionDlgManufacture : public CLAbstractDlgManufacture
{
private:
	AreconVisionDlgManufacture();
public:
	static AreconVisionDlgManufacture& instance();
public:
	CLAbstractDeviceSettingsDlg* createDlg(CLDevice* dev);
	bool canProduceDlg(CLDevice* dev) const;
private:
	QList<QString> mPossibleNames;

};

class AVSettingsDlg : public CLAbstractDeviceSettingsDlg
{
public:
	AVSettingsDlg(CLDevice* dev);
private:
	void initTabsOrder();
	void initImageQuality();
	void initExposure();
	void initAI();
	void initDN();
	
};

#endif //arecon_dlg_factory_h1733