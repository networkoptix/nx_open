#ifndef abstract_device_settings_dlg_h_1652
#define abstract_device_settings_dlg_h_1652

#include <QDialog>

class CLDevice;
class QTabWidget;
class QDialogButtonBox;

class CLAbstractDeviceSettingsDlg : public QDialog
{
public:

	CLAbstractDeviceSettingsDlg(CLDevice* dev);

	virtual ~CLAbstractDeviceSettingsDlg();

	virtual void init() {};
protected:
protected:
	CLDevice* mDevice;

	QTabWidget* mTabWidget;
	QDialogButtonBox* mButtonBox;
};

#endif //abstract_device_settings_dlg_h_1652