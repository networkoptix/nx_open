#ifndef device_settings_tab_h_2004
#define device_settings_tab_h_2004

#include <QWidget>
#include <QMap>
#include "../../device/param.h"


class CLDevice;
class CLAbstractDeviceSettingsDlg;

class CLDeviceSettingsTab : public QWidget
{
	Q_OBJECT
public:
	CLDeviceSettingsTab(CLAbstractDeviceSettingsDlg* dlg, CLDevice* dev, QString group);
	~CLDeviceSettingsTab();
public slots:
	void onOnOffStateChanged(int state);
	void onMinMaxStepChanged();
protected:
	const CLParam& param_helper() const;

	CLDevice* mDevice;
	QString mGroup;
	CLAbstractDeviceSettingsDlg* mDlg;
	
	
};

#endif //abstract_device_settings_tab_h_2004