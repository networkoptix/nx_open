#ifndef abstract_device_settings_dlg_h_1652
#define abstract_device_settings_dlg_h_1652

#include <QDialog>
#include "../../device/param.h"

class CLDevice;
class QTabWidget;
class QDialogButtonBox;

class CLAbstractDeviceSettingsDlg : public QDialog
{
	Q_OBJECT
public:

	CLAbstractDeviceSettingsDlg(CLDevice* dev);

	virtual ~CLAbstractDeviceSettingsDlg();

	CLDevice* getDevice() const;
public slots:
	virtual void setParam(const QString& name, const CLValue& val);


protected:
	virtual void init() {};
	

protected:
	CLDevice* mDevice;

	QTabWidget* mTabWidget;
	QDialogButtonBox* mButtonBox;
};

#endif //abstract_device_settings_dlg_h_1652