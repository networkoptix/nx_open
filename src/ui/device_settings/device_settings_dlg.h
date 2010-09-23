#ifndef abstract_device_settings_dlg_h_1652
#define abstract_device_settings_dlg_h_1652

#include <QDialog>
#include "../../device/param.h"
#include <QList>

class CLDevice;
class QTabWidget;
class QDialogButtonBox;
class CLAbstractSettingsWidget;
class QGroupBox;
class CLDeviceSettingsTab;


class CLAbstractDeviceSettingsDlg : public QDialog
{
	Q_OBJECT
public:

	CLAbstractDeviceSettingsDlg(CLDevice* dev);

	virtual ~CLAbstractDeviceSettingsDlg();

	CLDevice* getDevice() const;
	void putWidget(CLAbstractSettingsWidget* wgt);
	void putGroup(QGroupBox* group);

	void addTab(CLDeviceSettingsTab* tab);

public slots:
	virtual void setParam(const QString& name, const CLValue& val);
	virtual void onClose();
	virtual void onSuggestions();

protected:
	CLAbstractSettingsWidget* getWidgetByName(QString name) const;
	QGroupBox* getGroupByName(QString name) const;
	CLDeviceSettingsTab* tabByName(QString name) const;

protected:
	CLDevice* mDevice;

	QTabWidget* mTabWidget;
	QDialogButtonBox* mButtonBox;

	QList<CLAbstractSettingsWidget*> mWgtsLst;
	QList<QGroupBox*> mGroups;
	QList<CLDeviceSettingsTab*> mTabs;
};

#endif //abstract_device_settings_dlg_h_1652