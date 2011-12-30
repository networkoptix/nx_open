#ifndef abstract_device_settings_dlg_h_1652
#define abstract_device_settings_dlg_h_1652

#include "core/resource/resource.h"


class QnResource;
class QTabWidget;
class QDialogButtonBox;
class CLAbstractSettingsWidget;
class QGroupBox;
class CLDeviceSettingsTab;

class CLAbstractDeviceSettingsDlg : public QDialog
{
	Q_OBJECT
public:

	CLAbstractDeviceSettingsDlg(QnResourcePtr dev);

	virtual ~CLAbstractDeviceSettingsDlg();

	QnResourcePtr getDevice() const;
	void putWidget(CLAbstractSettingsWidget* wgt);
	void putGroup(QGroupBox* group);

public slots:
	virtual void setParam(const QString& name, const QVariant& val);
	virtual void onClose();
	virtual void onSuggestions();

    virtual void onNewtab(int index);

protected:
    void addTabWidget(CLDeviceSettingsTab* tab);
	CLAbstractSettingsWidget* getWidgetByName(QString name) const;
	QGroupBox* getGroupByName(QString name) const;
	CLDeviceSettingsTab* tabByName(QString name) const;

    QList<CLAbstractSettingsWidget*> getWidgetsBygroup(QString group) const;
protected:
	QnResourcePtr mDevice;

	QTabWidget* mTabWidget;
	QDialogButtonBox* mButtonBox;

	QList<CLAbstractSettingsWidget*> mWgtsLst;
	QList<QGroupBox*> mGroups;
	QList<CLDeviceSettingsTab*> mTabs;
};

#endif //abstract_device_settings_dlg_h_1652
