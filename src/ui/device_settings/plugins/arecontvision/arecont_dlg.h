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
	CLAbstractDeviceSettingsDlg* getDlg(CLDevice* dev);
	bool canProduce(CLDevice* dev) const;
private:
	QList<QString> mPossibleNames;

};

class AVSettingsDlg : public CLAbstractDeviceSettingsDlg
{
public:
	AVSettingsDlg(CLDevice* dev);
};

#endif //arecon_dlg_factory_h1733