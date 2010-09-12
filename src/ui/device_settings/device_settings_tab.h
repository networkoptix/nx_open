#ifndef device_settings_tab_h_2004
#define device_settings_tab_h_2004

#include <QWidget>
#include <QMap>

class CLDevice;

class CLDeviceSettingsTab : public QWidget
{
	Q_OBJECT
public:
	CLDeviceSettingsTab(CLDevice* dev, QString group);
	~CLDeviceSettingsTab();
public slots:
	void onOnOffStateChanged(int state);

protected:
	

	CLDevice* mDevice;
	QString mGroup;
	
	
};

#endif //abstract_device_settings_tab_h_2004