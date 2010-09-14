#ifndef abstract_device_settings_dlg_h_1652
#define abstract_device_settings_dlg_h_1652

#include <QDialog>
#include "../../device/param.h"

class CLDevice;
class QTabWidget;
class QDialogButtonBox;

class CLAbstractDeviceSettingsDlg : public QDialog
{
public:

	CLAbstractDeviceSettingsDlg(CLDevice* dev);

	virtual ~CLAbstractDeviceSettingsDlg();

	virtual bool setParam(const QString& name, const CLValue& val, bool applayToAll = false);
protected:
	virtual void init() {};
	

protected:
	CLDevice* mDevice;

	QTabWidget* mTabWidget;
	QDialogButtonBox* mButtonBox;
};

#endif //abstract_device_settings_dlg_h_1652