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
	Q_OBJECT
public:
	AVSettingsDlg(CLDevice* dev);
protected slots:
	virtual void onSuggestions();
	virtual void setParam(const QString& name, const CLValue& val);
    virtual void onClose();
private:

	void initTabsOrder();
	void initImageQuality();
	void initExposure();
	void initAI();
	void initDN();
	void initMD();
	void initAdmin();
	//==========
	void correctWgtsState();

};

#endif //arecon_dlg_factory_h1733
