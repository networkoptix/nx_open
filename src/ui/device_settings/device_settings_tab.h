#ifndef device_settings_tab_h_2004
#define device_settings_tab_h_2004

#include "../../device/param.h"

class CLDevice;
class CLAbstractDeviceSettingsDlg;

class CLDeviceSettingsTab : public QWidget
{

public:
	CLDeviceSettingsTab(CLAbstractDeviceSettingsDlg* dlg, QObject* handler, CLDevice* dev, QString group);
	~CLDeviceSettingsTab();
	QString name() const;
protected:

	CLDevice* mDevice;
	QString mGroup;
	QObject* mHandler;
	CLAbstractDeviceSettingsDlg *mDlg;

};

#endif //abstract_device_settings_tab_h_2004