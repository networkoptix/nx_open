#ifndef device_settings_tab_h_2004
#define device_settings_tab_h_2004

#include "core/resource/resource.h"



class QnResource;
class CLAbstractDeviceSettingsDlg;

class CLDeviceSettingsTab : public QWidget
{

public:
	CLDeviceSettingsTab(CLAbstractDeviceSettingsDlg* dlg, QObject* handler, QnResourcePtr dev, QString group);
	~CLDeviceSettingsTab();
	QString name() const;
protected:

	QnResourcePtr mDevice;
	QString mGroup;
	QObject* mHandler;
	CLAbstractDeviceSettingsDlg *mDlg;

};

#endif //abstract_device_settings_tab_h_2004
