#ifndef device_settings_tab_h_2004
#define device_settings_tab_h_2004

#include <QWidget>
#include <QMap>
#include "../../device/param.h"


class CLDevice;

class CLDeviceSettingsTab : public QWidget
{
	
public:
	CLDeviceSettingsTab(QObject* handler, CLDevice* dev, QString group);
	~CLDeviceSettingsTab();
protected:

	CLDevice* mDevice;
	QString mGroup;
	QObject* mHandler;
	
	
};

#endif //abstract_device_settings_tab_h_2004