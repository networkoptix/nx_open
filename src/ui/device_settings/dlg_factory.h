#ifndef dlg_factory__h_1712
#define dlg_factory__h_1712

#include <QList>

class CLDevice;
class CLAbstractDeviceSettingsDlg;

class CLAbstractDlgManufacture
{
protected:
	CLAbstractDlgManufacture(){};
public:
	virtual CLAbstractDeviceSettingsDlg* getDlg(CLDevice* dev) = 0;
	virtual bool canProduce(CLDevice* dev) const = 0;
};

class CLDeviceSettingsDlgFactory
{
private:
	CLDeviceSettingsDlgFactory(){};
public:
	static CLDeviceSettingsDlgFactory& instance();
public:
	CLAbstractDeviceSettingsDlg* getDlg(CLDevice* dev);
	void registerDlgManufacture(CLAbstractDlgManufacture* manufacture);
private:
	QList<CLAbstractDlgManufacture*> mMaunufactures;
};

#endif //dlg_factory__h_1712